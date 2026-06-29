'use client'

import { useEffect, useState } from 'react'
import { Card } from '@/components/ui/Card'
import { StatusDot } from '@/components/ui/StatusDot'
import type { AgentStatus as AgentStatusType } from '@/lib/types'

export function AgentStatus() {
  const [status, setStatus] = useState<AgentStatusType>({ running: false, activeRules: 0, executedToday: 0 })

  useEffect(() => {
    const poll = async () => {
      const response = await fetch('/api/agent/status')
      const payload = await response.json()
      setStatus(payload.status)
    }

    poll().catch(() => undefined)
    const intervalMs = 10000
    const timer = setInterval(() => {
      poll().catch(() => undefined)
    }, intervalMs)

    return () => clearInterval(timer)
  }, [])

  return (
    <Card>
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2 text-white">
          <StatusDot active={status.running} />
          <span className="font-semibold">{status.running ? 'Agent Active' : 'Agent Stopped'}</span>
        </div>
      </div>
      <div className="mt-4 grid grid-cols-2 gap-3 text-sm">
        <div className="rounded-lg bg-black/20 p-3">
          <p className="text-zinc-400">Active Rules</p>
          <p className="mt-1 text-xl font-semibold text-white">{status.activeRules}</p>
        </div>
        <div className="rounded-lg bg-black/20 p-3">
          <p className="text-zinc-400">Actions Today</p>
          <p className="mt-1 text-xl font-semibold text-white">{status.executedToday}</p>
        </div>
      </div>
    </Card>
  )
}
