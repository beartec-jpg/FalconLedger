import Link from 'next/link'
import { Card } from '@/components/ui/Card'
import { Badge } from '@/components/ui/Badge'
import { CHAIN_NAME, TOKEN_TICKER, FALCON_NETWORK_ID } from '@/lib/constants'

export default function Home() {
  return (
    <div className="space-y-6">
      <Card>
        <div className="flex flex-wrap items-center justify-between gap-3">
          <div>
            <h1 className="text-2xl font-semibold text-white">{CHAIN_NAME} Agent Wallet</h1>
            <p className="mt-2 max-w-2xl text-sm text-zinc-300">
              Standalone wallet infrastructure for Falcon Ledger operators. Standard wallet mode plus autonomous agent mode for programmable qXRP payments.
            </p>
          </div>
          <Badge variant="success">Network ID {FALCON_NETWORK_ID}</Badge>
        </div>
      </Card>

      <div className="grid gap-4 md:grid-cols-2">
        <Link href="/wallet" className="block">
          <Card className="h-full hover:border-violet-400/40">
            <h2 className="text-lg font-semibold text-white">Standard Wallet</h2>
            <p className="mt-2 text-sm text-zinc-300">Balance, send, receive, and recent transaction history for {TOKEN_TICKER}.</p>
          </Card>
        </Link>
        <Link href="/agent" className="block">
          <Card className="h-full hover:border-violet-400/40">
            <h2 className="text-lg font-semibold text-white">Agent Mode</h2>
            <p className="mt-2 text-sm text-zinc-300">Automate payments using watch, schedule, and condition rules with live agent telemetry.</p>
          </Card>
        </Link>
      </div>
    </div>
  )
}
