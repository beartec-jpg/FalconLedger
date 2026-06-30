import { AgentStatus } from '@/components/agent/AgentStatus'
import { AgentChat } from '@/components/agent/AgentChat'
import { RulesPanel } from '@/components/agent/RulesPanel'
import { AgentFeed } from '@/components/agent/AgentFeed'
import { AgentStats } from '@/components/agent/AgentStats'

export default function AgentPage() {
  return (
    <div className="space-y-4">
      <h1 className="text-2xl font-semibold text-white">Agent Mode</h1>
      <AgentStatus />
      <div className="grid gap-4 lg:grid-cols-2">
        <AgentChat />
        <AgentStats />
      </div>
      <div className="grid gap-4 lg:grid-cols-2">
        <RulesPanel />
        <AgentFeed />
      </div>
    </div>
  )
}
