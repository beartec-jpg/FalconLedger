'use client'

import { useEffect, useState } from 'react'
import { Card } from '@/components/ui/Card'

interface Stats {
  totalPaymentsAutomated: number
  totalQxrpMoved: string
}

export function AgentStats() {
  const [stats, setStats] = useState<Stats>({ totalPaymentsAutomated: 0, totalQxrpMoved: '0.000000' })

  useEffect(() => {
    const fetchStats = async () => {
      const response = await fetch('/api/agent/status')
      const payload = await response.json()
      setStats(payload.stats)
    }

    fetchStats().catch(() => undefined)
  }, [])

  return (
    <Card>
      <h3 className="text-base font-semibold text-white">Agent Stats</h3>
      <div className="mt-4 grid grid-cols-2 gap-3 text-sm">
        <div className="rounded-lg bg-black/20 p-3">
          <p className="text-zinc-400">Total Payments Automated</p>
          <p className="mt-1 text-xl font-semibold text-white">{stats.totalPaymentsAutomated}</p>
        </div>
        <div className="rounded-lg bg-black/20 p-3">
          <p className="text-zinc-400">Total qXRP Moved</p>
          <p className="mt-1 text-xl font-semibold text-white">{stats.totalQxrpMoved}</p>
        </div>
      </div>
    </Card>
  )
}
