import Link from 'next/link'
import { Wallet, Bot } from 'lucide-react'
import { CHAIN_NAME } from '@/lib/constants'

export function Navbar() {
  return (
    <nav className="border-b border-white/10 bg-background/80 backdrop-blur">
      <div className="mx-auto flex max-w-6xl items-center justify-between px-6 py-4">
        <Link href="/" className="flex items-center gap-2 text-sm font-semibold text-white">
          <img src="/falcon-logo.svg" alt="Falcon Ledger" className="size-6" />
          {CHAIN_NAME} Agent Wallet
        </Link>
        <div className="flex items-center gap-2">
          <Link href="/wallet" className="flex items-center gap-2 rounded-lg px-3 py-2 text-sm text-zinc-200 hover:bg-white/5">
            <Wallet className="size-4" /> Wallet
          </Link>
          <Link href="/agent" className="flex items-center gap-2 rounded-lg px-3 py-2 text-sm text-zinc-200 hover:bg-white/5">
            <Bot className="size-4" /> Agent Mode
          </Link>
        </div>
      </div>
    </nav>
  )
}
