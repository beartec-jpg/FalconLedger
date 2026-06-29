import { NextResponse } from 'next/server'
import { agentEngine } from '@/lib/agent-engine'

export const runtime = 'nodejs'

export async function GET() {
  return NextResponse.json({
    status: agentEngine.getStatus(),
    actions: agentEngine.getActions(50),
    stats: agentEngine.getStats()
  })
}
