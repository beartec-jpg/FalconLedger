'use client'

import { useEffect, useState } from 'react'
import { Card } from '@/components/ui/Card'
import type { Transaction } from '@/lib/types'

export function TransactionFeed({ address }: { address: string }) {
  const [transactions, setTransactions] = useState<Transaction[]>([])

  useEffect(() => {
    const fetchHistory = async () => {
      const response = await fetch(`/api/wallet/history?address=${encodeURIComponent(address)}`)
      const payload = await response.json()
      setTransactions(payload.transactions ?? [])
    }

    fetchHistory().catch(() => setTransactions([]))
  }, [address])

  return (
    <Card>
      <h3 className="text-base font-semibold text-white">Recent Transactions</h3>
      <div className="mt-4 space-y-2">
        {transactions.length === 0 ? (
          <p className="text-sm text-zinc-400">No transactions yet.</p>
        ) : (
          transactions.slice(0, 10).map((transaction) => (
            <div key={transaction.hash} className="rounded-lg border border-white/10 bg-black/20 p-3 text-xs text-zinc-300">
              <p className="font-medium text-white">{transaction.type}</p>
              <p className="mt-1 break-all">Hash: {transaction.hash}</p>
              <p className="mt-1">Amount: {transaction.amount} drops</p>
              <p className="mt-1 text-zinc-500">{new Date(transaction.timestamp).toLocaleString()}</p>
            </div>
          ))
        )}
      </div>
    </Card>
  )
}
