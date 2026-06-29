'use client'

import { useState } from 'react'
import { Card } from '@/components/ui/Card'
import { Button } from '@/components/ui/Button'

export function SendPayment() {
  const [to, setTo] = useState('')
  const [amount, setAmount] = useState('')
  const [result, setResult] = useState('')

  const submit = async (event: React.FormEvent) => {
    event.preventDefault()

    const drops = Math.max(0, Math.round(Number(amount || '0') * 1_000_000)).toString()
    const txBlob = JSON.stringify({
      TransactionType: 'Payment',
      Account: 'rFalconAgentPrototype',
      Destination: to,
      Amount: drops
    })

    const response = await fetch('/api/wallet/send', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ txBlob })
    })
    const payload = await response.json()
    setResult(payload.txHash ? `Submitted: ${payload.txHash}` : payload.error || 'Unable to submit')
  }

  return (
    <Card>
      <h3 className="text-base font-semibold text-white">Send qXRP</h3>
      <form onSubmit={submit} className="mt-4 space-y-3">
        <input
          className="w-full rounded-lg border border-white/10 bg-black/20 px-3 py-2 text-sm text-white outline-none focus:border-accent"
          placeholder="Recipient address"
          value={to}
          onChange={(event) => setTo(event.target.value)}
          required
        />
        <input
          className="w-full rounded-lg border border-white/10 bg-black/20 px-3 py-2 text-sm text-white outline-none focus:border-accent"
          placeholder="Amount (qXRP)"
          type="number"
          min="0"
          step="0.000001"
          value={amount}
          onChange={(event) => setAmount(event.target.value)}
          required
        />
        <Button type="submit" className="w-full">Send Payment</Button>
      </form>
      {result ? <p className="mt-3 break-all text-xs text-zinc-400">{result}</p> : null}
    </Card>
  )
}
