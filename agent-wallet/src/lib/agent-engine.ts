import type { AgentAction, AgentRule, AgentStatus } from '@/lib/types'

type ActionListener = (action: AgentAction) => void

class AgentEngine {
  private rules = new Map<string, AgentRule>()
  private actions: AgentAction[] = []
  private listeners = new Set<ActionListener>()
  private running = false
  private totalMovedDrops = 0

  addRule(rule: AgentRule): AgentRule {
    this.rules.set(rule.id, rule)
    return rule
  }

  removeRule(id: string): void {
    this.rules.delete(id)
  }

  updateRule(id: string, updates: Partial<AgentRule>): AgentRule | undefined {
    const existing = this.rules.get(id)
    if (!existing) {
      return undefined
    }

    const updated = { ...existing, ...updates }
    this.rules.set(id, updated)
    return updated
  }

  getRules(): AgentRule[] {
    return Array.from(this.rules.values())
  }

  getActions(limit = 25): AgentAction[] {
    return this.actions.slice(0, limit)
  }

  getStatus(): AgentStatus {
    const today = new Date().toISOString().slice(0, 10)
    const executedToday = this.actions.filter(
      (action) => action.status === 'executed' && action.timestamp.startsWith(today)
    ).length

    return {
      running: this.running,
      activeRules: this.getRules().filter((rule) => rule.enabled).length,
      executedToday,
      lastAction: this.actions[0]
    }
  }

  getStats() {
    return {
      totalPaymentsAutomated: this.actions.filter((action) => action.status === 'executed').length,
      totalQxrpMoved: (this.totalMovedDrops / 1_000_000).toFixed(6)
    }
  }

  onAction(listener: ActionListener): () => void {
    this.listeners.add(listener)
    return () => {
      this.listeners.delete(listener)
    }
  }

  executeRule(rule: AgentRule): AgentAction {
    const pending: AgentAction = {
      id: `action-${Date.now()}`,
      ruleId: rule.id,
      timestamp: new Date().toISOString(),
      description: `Executing rule "${rule.name}": ${rule.action}`,
      status: 'pending'
    }

    this.actions.unshift(pending)
    this.emit(pending)

    const executed: AgentAction = {
      ...pending,
      txHash: `TX-${Math.random().toString(36).slice(2, 12).toUpperCase()}`,
      status: 'executed',
      description: `Executed rule "${rule.name}": ${rule.action}`
    }

    const amountMatch = rule.action.match(/(\d+(?:\.\d+)?)\s*qxrp/i)
    if (amountMatch) {
      this.totalMovedDrops += Math.round(parseFloat(amountMatch[1]) * 1_000_000)
    }

    this.actions[0] = executed
    this.rules.set(rule.id, { ...rule, lastTriggered: new Date().toISOString() })
    this.emit(executed)

    return executed
  }

  start(): void {
    this.running = true
  }

  stop(): void {
    this.running = false
  }

  private emit(action: AgentAction): void {
    this.listeners.forEach((listener) => listener(action))
  }
}

export const agentEngine = new AgentEngine()
