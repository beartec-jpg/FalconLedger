import { NextRequest, NextResponse } from 'next/server'
import { falconClient } from '@/lib/falcon-client'
import type { Transaction } from '@/lib/types'

export const runtime = 'nodejs'

const mockTransactions: Transaction[] = Array.from({ length: 10 }).map((_, index) => ({
  hash: `MOCKTX-${index + 1}`,
  type: 'Payment',
  from: 'rFalconSource111111111111111111111',
  to: 'rFalconDestination1111111111111111',
  amount: `${(index + 1) * 1000000}`,
  fee: '12',
  ledgerIndex: 500000 + index,
  timestamp: new Date(Date.now() - index * 60000).toISOString(),
  status: 'validated'
}))

export async function GET(request: NextRequest) {
  const address = request.nextUrl.searchParams.get('address')

  if (!address) {
    return NextResponse.json({ error: 'address is required' }, { status: 400 })
  }

  try {
    const transactions = await falconClient.getAccountTransactions(address)
    return NextResponse.json({ transactions: transactions.slice(0, 10) })
  } catch {
    return NextResponse.json({ transactions: mockTransactions, source: 'mock' })
  }
}
