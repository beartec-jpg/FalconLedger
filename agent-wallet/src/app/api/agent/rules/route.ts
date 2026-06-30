import { NextRequest, NextResponse } from 'next/server'
import { agentEngine } from '@/lib/agent-engine'
import type { AgentRule } from '@/lib/types'

export const runtime = 'nodejs'

export async function GET() {
  return NextResponse.json({ rules: agentEngine.getRules() })
}

export async function POST(request: NextRequest) {
  const body = await request.json().catch(() => ({}))

  const rule: AgentRule = {
    id: body.id || `rule-${Date.now()}`,
    name: body.name || 'New rule',
    type: body.type || 'watch',
    condition: body.condition || 'Manual condition',
    action: body.action || 'Notify operator',
    enabled: body.enabled ?? true
  }

  agentEngine.addRule(rule)
  return NextResponse.json({ rule })
}

export async function PATCH(request: NextRequest) {
  const body = await request.json().catch(() => ({}))
  const id = String(body.id || '')

  if (!id) {
    return NextResponse.json({ error: 'id is required' }, { status: 400 })
  }

  const updated = agentEngine.updateRule(id, { enabled: Boolean(body.enabled) })

  if (!updated) {
    return NextResponse.json({ error: 'rule not found' }, { status: 404 })
  }

  return NextResponse.json({ rule: updated })
}

export async function DELETE(request: NextRequest) {
  const id = request.nextUrl.searchParams.get('id')
  if (!id) {
    return NextResponse.json({ error: 'id is required' }, { status: 400 })
  }

  agentEngine.removeRule(id)
  return NextResponse.json({ ok: true })
}
