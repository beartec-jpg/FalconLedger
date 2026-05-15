// POST /api/faucet
// Body: { account: string }
// Returns: { txHash, amount, account, reset } | { error, reset? }

import { NextRequest, NextResponse } from 'next/server'
import { checkRateLimit } from '@/lib/rate-limit'
import { getAccountInfo, getLedgerIndex, submitTx } from '@/lib/rpc'
import { signPayment, dropsFromQxrp } from '@/lib/xrpl-sign'
import { isValidClassicAddress } from 'xrpl'

export const runtime = 'nodejs'
export const dynamic = 'force-dynamic'

const FAUCET_ACCOUNT = process.env.FAUCET_ACCOUNT ?? ''
const FAUCET_SECRET  = process.env.FAUCET_SECRET  ?? ''
const DRIP_AMOUNT    = parseFloat(process.env.DRIP_AMOUNT_QXRP ?? '100')

function ip(req: NextRequest): string {
  return (
    req.headers.get('x-forwarded-for')?.split(',')[0]?.trim() ??
    req.headers.get('x-real-ip') ??
    'unknown'
  )
}

function err(msg: string, status = 400, extra?: Record<string, unknown>) {
  return NextResponse.json({ error: msg, ...extra }, { status })
}

export async function POST(req: NextRequest) {
  // ── Parse body ────────────────────────────────────────────────────────────
  let account: string
  try {
    const body = await req.json()
    account = (body.account ?? '').toString().trim()
  } catch {
    return err('Invalid JSON body')
  }

  if (!account) return err('Missing "account" field')
  if (!isValidClassicAddress(account)) return err('Invalid qXRP address')

  // ── Rate limit: by IP and by destination account ──────────────────────────
  const clientIp = ip(req)
  const [ipLimit, acctLimit] = await Promise.all([
    checkRateLimit(`ip:${clientIp}`),
    checkRateLimit(`acct:${account}`),
  ])

  if (!ipLimit.success) {
    return err(
      'Rate limit exceeded for your IP. Try again later.',
      429,
      { reset: ipLimit.reset }
    )
  }
  if (!acctLimit.success) {
    return err(
      'Rate limit exceeded for this account. Try again later.',
      429,
      { reset: acctLimit.reset }
    )
  }

  // ── Validate faucet configuration ─────────────────────────────────────────
  if (!FAUCET_ACCOUNT || !FAUCET_SECRET) {
    console.error('Faucet FAUCET_ACCOUNT / FAUCET_SECRET not configured')
    return err('Faucet not configured', 500)
  }

  // ── Fetch current sequence + ledger ──────────────────────────────────────
  let sequence: number
  let lastLedgerSequence: number
  try {
    const [acctInfo, ledger] = await Promise.all([
      getAccountInfo(FAUCET_ACCOUNT),
      getLedgerIndex(),
    ])
    sequence = acctInfo.account_data.Sequence
    lastLedgerSequence = ledger + 10
  } catch (e) {
    console.error('RPC error fetching account info:', e)
    return err('Cannot reach qXRP node. Try again shortly.', 503)
  }

  // ── Sign and submit ───────────────────────────────────────────────────────
  const amountDrops = dropsFromQxrp(DRIP_AMOUNT)
  let tx_blob: string
  let txHash: string
  try {
    const signed = signPayment({
      from: FAUCET_ACCOUNT,
      secret: FAUCET_SECRET,
      to: account,
      amountDrops,
      sequence,
      lastLedgerSequence,
    })
    tx_blob = signed.tx_blob
    txHash  = signed.hash
  } catch (e) {
    console.error('Signing error:', e)
    return err('Transaction signing failed', 500)
  }

  let engineResult: string
  let engineMsg: string
  try {
    const result = await submitTx(tx_blob)
    engineResult = result.engine_result
    engineMsg    = result.engine_result_message
    txHash       = result.tx_json?.hash ?? txHash
  } catch (e) {
    console.error('Submit error:', e)
    return err('Transaction submission failed', 503)
  }

  // tesSUCCESS or terQUEUED are acceptable
  const accepted = engineResult.startsWith('tes') || engineResult === 'terQUEUED'
  if (!accepted) {
    return err(`Transaction rejected: ${engineResult} — ${engineMsg}`, 422)
  }

  return NextResponse.json({
    txHash,
    amount: DRIP_AMOUNT,
    account,
    engine_result: engineResult,
    reset: acctLimit.reset,
  })
}
