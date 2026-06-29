'use client'

import { useCallback, useEffect, useState } from 'react'
import { Card } from '@/components/ui/Card'
import { Button } from '@/components/ui/Button'
import type { AgentRule } from '@/lib/types'

export function RulesPanel() {
  const [rules, setRules] = useState<AgentRule[]>([])

  const loadRules = useCallback(async () => {
    const response = await fetch('/api/agent/rules')
    const payload = await response.json()
    setRules(payload.rules ?? [])
  }, [])

  useEffect(() => {
    loadRules().catch(() => setRules([]))
  }, [loadRules])

  const toggleRule = async (rule: AgentRule) => {
    await fetch('/api/agent/rules', {
      method: 'PATCH',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: rule.id, enabled: !rule.enabled })
    })
    await loadRules()
  }

  const deleteRule = async (id: string) => {
    await fetch(`/api/agent/rules?id=${encodeURIComponent(id)}`, { method: 'DELETE' })
    await loadRules()
  }

  return (
    <Card>
      <h3 className="text-base font-semibold text-white">Rules Panel</h3>
      <div className="mt-4 space-y-3">
        {rules.length === 0 ? <p className="text-sm text-zinc-400">No rules configured.</p> : null}
        {rules.map((rule) => (
          <div key={rule.id} className="rounded-lg border border-white/10 bg-black/20 p-3">
            <div className="flex items-center justify-between gap-2">
              <p className="text-sm font-medium text-white">{rule.name}</p>
              <div className="flex items-center gap-2">
                <Button variant="ghost" onClick={() => toggleRule(rule)}>{rule.enabled ? 'Disable' : 'Enable'}</Button>
                <Button variant="danger" onClick={() => deleteRule(rule.id)}>Delete</Button>
              </div>
            </div>
            <p className="mt-1 text-xs text-zinc-400">{rule.condition}</p>
            <p className="mt-1 text-xs text-zinc-500">Action: {rule.action}</p>
          </div>
        ))}
      </div>
    </Card>
  )
}
