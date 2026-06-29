import { NextRequest, NextResponse } from 'next/server'
import { falconClient } from '@/lib/falcon-client'

export const runtime = 'nodejs'

export async function GET(request: NextRequest) {
  const address = request.nextUrl.searchParams.get('address')

  if (!address) {
    return NextResponse.json({ error: 'address is required' }, { status: 400 })
  }

  try {
    const balance = await falconClient.getBalance(address)
    return NextResponse.json({ address, balance })
  } catch {
    return NextResponse.json({ address, balance: '250000000', source: 'mock' })
  }
}
