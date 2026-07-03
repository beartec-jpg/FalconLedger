#!/usr/bin/env node
/**
 * Deploy FalconCollateralLock to Sepolia.
 *
 * Usage:
 *   cd qXRP/scripts
 *   npm install ethers solc@0.8.28
 *   PRIVATE_KEY=0x... node deploy-falcon-lock.js
 *
 * Optional:
 *   RPC_URL=https://rpc.sepolia.org
 *   USDC_TOKEN=0x1c7D4B196Cb0C7B01d743Fbc6116a902379C7238
 */

const fs = require('fs')
const path = require('path')
const { ethers } = require('ethers')

const USDC_SEPOLIA = process.env.USDC_TOKEN || '0x1c7D4B196Cb0C7B01d743Fbc6116a902379C7238'
const RPC_URL = process.env.RPC_URL || 'https://rpc.sepolia.org'

async function main() {
  const privateKey = process.env.PRIVATE_KEY
  if (!privateKey) {
    console.error('Set PRIVATE_KEY (Sepolia wallet with ETH for gas)')
    process.exit(1)
  }

  const solc = require('solc')
  const sourcePath = path.join(__dirname, '..', 'contracts', 'FalconCollateralLock.sol')
  const source = fs.readFileSync(sourcePath, 'utf8')

  const input = {
    language: 'Solidity',
    sources: { 'FalconCollateralLock.sol': { content: source } },
    settings: {
      optimizer: { enabled: true, runs: 200 },
      outputSelection: { '*': { '*': ['abi', 'evm.bytecode.object'] } },
    },
  }

  console.log('Compiling FalconCollateralLock.sol...')
  const output = JSON.parse(solc.compile(JSON.stringify(input)))
  if (output.errors) {
    const fatal = output.errors.filter((e) => e.severity === 'error')
    if (fatal.length) {
      console.error(fatal.map((e) => e.formattedMessage).join('\n'))
      process.exit(1)
    }
  }

  const compiled = output.contracts['FalconCollateralLock.sol'].FalconCollateralLock
  const bytecode = '0x' + compiled.evm.bytecode.object
  const abi = compiled.abi

  const provider = new ethers.JsonRpcProvider(RPC_URL)
  const wallet = new ethers.Wallet(privateKey, provider)
  const balance = await provider.getBalance(wallet.address)

  console.log(`RPC:        ${RPC_URL}`)
  console.log(`Deployer:   ${wallet.address}`)
  console.log(`Balance:    ${ethers.formatEther(balance)} ETH`)
  console.log(`USDC token: ${USDC_SEPOLIA}`)

  if (balance === 0n) {
    console.error('No Sepolia ETH for gas')
    process.exit(1)
  }

  const factory = new ethers.ContractFactory(abi, bytecode, wallet)
  console.log('Deploying...')
  const contract = await factory.deploy(USDC_SEPOLIA)
  await contract.waitForDeployment()
  const address = await contract.getAddress()

  console.log('\n========================================')
  console.log(`FalconCollateralLock: ${address}`)
  console.log('========================================\n')
  console.log('Add to qXRP-faucet-wallet .env.local:')
  console.log(`SEPOLIA_LOCK_CONTRACT=${address}`)
  console.log(`NEXT_PUBLIC_SEPOLIA_LOCK_CONTRACT=${address}`)
  console.log('\nSepolia USDC (approve this spender before deposit):')
  console.log(USDC_SEPOLIA)
}

main().catch((err) => {
  console.error(err)
  process.exit(1)
})