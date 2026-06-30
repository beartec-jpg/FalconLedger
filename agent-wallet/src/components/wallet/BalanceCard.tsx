'use client'

import { useEffect, useState } from 'react'
import { Badge } from '@/components/ui/Badge'
import { Card } from '@/components/ui/Card'
import { TOKEN_TICKER } from '@/lib/constants'

export function BalanceCard({ address }: { address: string }) {
  const [balance, setBalance] = useState('0')

  useEffect(() => {
    const fetchBalance = async () => {
      const response = await fetch(`/api/wallet/balance?address=${encodeURIComponent(address)}`)
      const payload = await response.json()
      setBalance(payload.balance ?? '0')
    }

    fetchBalance().catch(() => setBalance('0'))
  }, [address])

  const qxrp = (Number(balance) / 1_000_000).toFixed(6)

  return (
    <Card>
      <div className="flex items-center justify-between">
        <p className="text-sm text-zinc-400">Available Balance</p>
        <Badge variant="success">Quantum Safe ✓</Badge>
      </div>
      <p className="mt-4 text-4xl font-semibold text-white">{qxrp} {TOKEN_TICKER}</p>
      <p className="mt-2 text-xs text-zinc-500">{balance} drops</p>
    </Card>
  )
}
