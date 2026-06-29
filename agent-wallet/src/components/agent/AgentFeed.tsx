'use client'

import { useEffect, useState } from 'react'
import { Card } from '@/components/ui/Card'
import type { AgentAction } from '@/lib/types'

export function AgentFeed() {
  const [actions, setActions] = useState<AgentAction[]>([])

  useEffect(() => {
    const poll = async () => {
      const response = await fetch('/api/agent/status')
      const payload = await response.json()
      setActions(payload.actions ?? [])
    }

    poll().catch(() => undefined)
    const timer = setInterval(() => {
      poll().catch(() => undefined)
    }, 10000)

    return () => clearInterval(timer)
  }, [])

  return (
    <Card>
      <h3 className="text-base font-semibold text-white">Live Agent Feed</h3>
      <div className="mt-4 max-h-80 space-y-2 overflow-y-auto">
        {actions.length === 0 ? <p className="text-sm text-zinc-400">No agent actions yet.</p> : null}
        {actions.map((action) => (
          <div key={action.id} className="rounded-lg border border-white/10 bg-black/20 p-3 text-xs">
            <p className="text-white">{action.description}</p>
            <p className="mt-1 text-zinc-500">{new Date(action.timestamp).toLocaleString()}</p>
            {action.txHash ? <p className="mt-1 break-all text-violet-200">TX: {action.txHash}</p> : null}
          </div>
        ))}
      </div>
    </Card>
  )
}
