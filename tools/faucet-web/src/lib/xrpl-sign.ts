// Transaction signing for qXRP payments via the Falcon signing proxy.
// The qXRP chain requires FalconPublicKey + FalconSignature fields that
// xrpl.js cannot produce — signing is delegated to the node1 admin RPC
// through a small HTTP proxy.

const DROPS_PER_QXRP = 1_000_000n

export function dropsFromQxrp(qxrp: number): string {
  return (BigInt(Math.round(qxrp)) * DROPS_PER_QXRP).toString()
}

export interface SignedPayment {
  tx_blob: string
  hash: string
}

export async function signPayment(opts: {
  from: string
  secret: string
  to: string
  amountDrops: string
  sequence: number
  lastLedgerSequence: number
  fee?: string
}): Promise<SignedPayment> {
  const { from, secret, to, amountDrops, sequence, lastLedgerSequence, fee = '12' } = opts

  const proxyUrl   = process.env.SIGNER_PROXY_URL
  const proxyToken = process.env.SIGNER_PROXY_TOKEN

  if (!proxyUrl) {
    throw new Error('SIGNER_PROXY_URL is not configured')
  }

  const tx_json = {
    TransactionType: 'Payment',
    Account: from,
    Destination: to,
    Amount: amountDrops,
    Fee: fee,
    Sequence: sequence,
    LastLedgerSequence: lastLedgerSequence,
    Flags: 0,
  }

  const res = await fetch(`${proxyUrl}/sign`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      ...(proxyToken ? { Authorization: `Bearer ${proxyToken}` } : {}),
    },
    body: JSON.stringify({ tx_json, secret }),
    cache: 'no-store',
  })

  if (!res.ok) {
    const text = await res.text().catch(() => res.statusText)
    throw new Error(`Signing proxy error ${res.status}: ${text}`)
  }

  const data = await res.json()
  return { tx_blob: data.tx_blob, hash: data.hash }
}
