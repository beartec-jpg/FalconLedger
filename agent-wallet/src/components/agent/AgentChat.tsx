'use client'

import { useState } from 'react'
import { Card } from '@/components/ui/Card'
import { Button } from '@/components/ui/Button'

interface ChatMessage {
  id: string
  role: 'user' | 'agent'
  content: string
}

export function AgentChat() {
  const [input, setInput] = useState('')
  const [messages, setMessages] = useState<ChatMessage[]>([])

  const submit = async (event: React.FormEvent) => {
    event.preventDefault()
    if (!input.trim()) return

    const current = input
    setMessages((prev) => [...prev, { id: `u-${Date.now()}`, role: 'user', content: current }])
    setInput('')

    const response = await fetch('/api/agent/command', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ command: current })
    })

    const payload = await response.json()
    setMessages((prev) => [...prev, { id: `a-${Date.now()}`, role: 'agent', content: payload.message ?? 'Done.' }])
  }

  return (
    <Card>
      <h3 className="text-base font-semibold text-white">Agent Chat</h3>
      <p className="mt-1 text-xs text-zinc-400">Try: Send 10 qXRP to rExample every day at 9am</p>
      <div className="mt-4 max-h-60 space-y-2 overflow-y-auto rounded-lg border border-white/10 bg-black/20 p-3">
        {messages.length === 0 ? <p className="text-sm text-zinc-500">No commands yet.</p> : null}
        {messages.map((message) => (
          <div
            key={message.id}
            className={`rounded-lg p-2 text-sm ${message.role === 'user' ? 'bg-accent/20 text-violet-100' : 'bg-white/5 text-zinc-200'}`}
          >
            {message.content}
          </div>
        ))}
      </div>
      <form onSubmit={submit} className="mt-3 flex gap-2">
        <input
          className="flex-1 rounded-lg border border-white/10 bg-black/20 px-3 py-2 text-sm text-white outline-none focus:border-accent"
          value={input}
          onChange={(event) => setInput(event.target.value)}
          placeholder="Describe an automated payment rule..."
        />
        <Button type="submit">Send</Button>
      </form>
    </Card>
  )
}
