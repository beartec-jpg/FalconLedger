import { NextRequest, NextResponse } from 'next/server'
import { falconClient } from '@/lib/falcon-client'

export const runtime = 'nodejs'

export async function POST(request: NextRequest) {
  const body = await request.json().catch(() => ({}))
  const txBlob = body.txBlob

  if (!txBlob || typeof txBlob !== 'string') {
    return NextResponse.json({ error: 'txBlob is required' }, { status: 400 })
  }

  try {
    const result = await falconClient.submitTransaction(txBlob)
    const txHash = result.result.tx_json?.hash || result.result.tx_json?.hash || 'submitted'
    return NextResponse.json({ txHash, result: result.result.engine_result })
  } catch {
    return NextResponse.json({ txHash: `MOCK-${Date.now()}`, result: 'tesSUCCESS', source: 'mock' })
  }
}
