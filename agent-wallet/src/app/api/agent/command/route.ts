import { NextRequest, NextResponse } from 'next/server'
import { agentEngine } from '@/lib/agent-engine'
import type { AgentRule } from '@/lib/types'

export const runtime = 'nodejs'

function parseCommand(command: string): AgentRule | null {
  const sendMatch = command.match(/send\s+(\d+(?:\.\d+)?)\s+q?xrp\s+to\s+([A-Za-z0-9]+)/i)

  if (!sendMatch) {
    return null
  }

  const amount = sendMatch[1]
  const destination = sendMatch[2]
  const scheduleMatch = command.match(/every\s+day\s+at\s+([^,.;]+)/i)

  return {
    id: `rule-${Date.now()}`,
    name: `Send ${amount} qXRP`,
    type: scheduleMatch ? 'schedule' : 'condition',
    condition: scheduleMatch ? `Daily at ${scheduleMatch[1].trim()}` : 'On demand',
    action: `Send ${amount} qXRP to ${destination}`,
    enabled: true
  }
}

export async function POST(request: NextRequest) {
  const body = await request.json().catch(() => ({}))
  const command = String(body.command || '').trim()

  if (!command) {
    return NextResponse.json({ error: 'command is required' }, { status: 400 })
  }

  if (/^start\s+agent$/i.test(command)) {
    agentEngine.start()
    return NextResponse.json({ message: 'Agent started and monitoring Falcon Ledger wallets.' })
  }

  if (/^stop\s+agent$/i.test(command)) {
    agentEngine.stop()
    return NextResponse.json({ message: 'Agent stopped. No autonomous actions will run.' })
  }

  const rule = parseCommand(command)

  if (!rule) {
    return NextResponse.json({ message: 'Command received. Try: Send 10 qXRP to rAddress every day at 9am.' })
  }

  agentEngine.addRule(rule)
  agentEngine.start()
  const action = agentEngine.executeRule(rule)

  return NextResponse.json({
    message: `Rule created and executed: ${rule.name}`,
    rule,
    action
  })
}
