#!/usr/bin/env node
/**
 * Confirm FalconCollateralLock bridge-out on EVM (used by bridge-withdraw-relay.py).
 *
 * Multi-sig contracts: calls confirmWithdraw (each owner key must confirm until threshold).
 * Legacy 1-of-1: confirmWithdraw works when required==1; withdraw() also works as alias.
 *
 * Usage:
 *   SEPOLIA_OWNER_PRIVATE_KEY=0x... node bridge-sepolia-withdraw.js \
 *     --lock 0x... --amount-raw 1000000 --recipient 0x... \
 *     --withdrawal-id 0x... --falcon-account r... --falcon-tx ABC...
 */

const { ethers } = require('ethers')

const LOCK_ABI = [
  'function confirmWithdraw(uint256 amount, address recipient, bytes32 withdrawalId, string falconAccount, string falconTxHash) external',
  'function withdraw(uint256 amount, address recipient, bytes32 withdrawalId, string falconAccount, string falconTxHash) external',
  'function required() view returns (uint256)',
  'function confirmationCount(bytes32) view returns (uint256)',
  'function withdrawOpHash(uint256,address,bytes32,string,string) view returns (bytes32)',
  'function processedWithdrawals(bytes32) view returns (bool)',
]

const RPC_FALLBACKS = [
  process.env.SEPOLIA_RPC_URL || 'https://ethereum-sepolia-rpc.publicnode.com',
  'https://1rpc.io/sepolia',
  'https://sepolia.drpc.org',
]

function parseArgs(argv) {
  const out = {}
  for (let i = 2; i < argv.length; i += 2) {
    const key = argv[i]?.replace(/^--/, '')
    out[key] = argv[i + 1]
  }
  return out
}

async function provider() {
  for (const url of RPC_FALLBACKS) {
    try {
      const p = new ethers.JsonRpcProvider(url, 11155111, { staticNetwork: true })
      await p.getBlockNumber()
      return p
    } catch {
      /* try next */
    }
  }
  throw new Error('Sepolia RPC unavailable')
}

async function main() {
  const args = parseArgs(process.argv)
  const pk = process.env.SEPOLIA_OWNER_PRIVATE_KEY || process.env.PRIVATE_KEY
  if (!pk) throw new Error('SEPOLIA_OWNER_PRIVATE_KEY or PRIVATE_KEY required')

  const lock = args.lock
  const amountRaw = args['amount-raw']
  const recipient = args.recipient
  const withdrawalId = args['withdrawal-id']
  const falconAccount = args['falcon-account']
  const falconTx = args['falcon-tx']

  for (const [name, val] of Object.entries({
    lock,
    'amount-raw': amountRaw,
    recipient,
    'withdrawal-id': withdrawalId,
    'falcon-account': falconAccount,
    'falcon-tx': falconTx,
  })) {
    if (!val) throw new Error(`missing --${name}`)
  }

  const p = await provider()
  const signer = new ethers.Wallet(pk, p)
  const contract = new ethers.Contract(lock, LOCK_ABI, signer)

  if (await contract.processedWithdrawals(withdrawalId)) {
    console.log('already-processed')
    return
  }

  let tx
  try {
    tx = await contract.confirmWithdraw(
      BigInt(amountRaw),
      recipient,
      withdrawalId,
      falconAccount,
      falconTx,
    )
  } catch (e) {
    // Fallback for pre-multisig deployments that only expose withdraw().
    tx = await contract.withdraw(
      BigInt(amountRaw),
      recipient,
      withdrawalId,
      falconAccount,
      falconTx,
    )
  }

  const rc = await tx.wait()
  if (!rc || rc.status !== 1) throw new Error('withdraw transaction failed')

  let note = rc.hash
  try {
    const need = await contract.required()
    const opHash = await contract.withdrawOpHash(
      BigInt(amountRaw),
      recipient,
      withdrawalId,
      falconAccount,
      falconTx,
    )
    const count = await contract.confirmationCount(opHash)
    const done = await contract.processedWithdrawals(withdrawalId)
    note = `${rc.hash} conf=${count}/${need} released=${done}`
  } catch {
    /* optional status */
  }
  console.log(note)
}

main().catch((err) => {
  console.error(err.message || err)
  process.exit(1)
})
