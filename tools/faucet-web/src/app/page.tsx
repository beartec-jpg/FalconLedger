'use client'

import { useState, useEffect, useCallback } from 'react'

type Tab = 'faucet' | 'wallet' | 'launch'

// ─── Types ────────────────────────────────────────────────────────────────────

interface NetworkStatus {
  online: boolean
  state?: string
  ledger?: number
  peers?: number
  loadFactor?: number
}

interface DripResult {
  txHash: string
  amount: number
  account: string
  reset: string
}

// ─── Constants ───────────────────────────────────────────────────────────────

const NETWORK_NAME = process.env.NEXT_PUBLIC_NETWORK_NAME ?? 'qXRP Testnet'
const NETWORK_ID   = process.env.NEXT_PUBLIC_NETWORK_ID   ?? '999'
const EXPLORER_URL = process.env.NEXT_PUBLIC_EXPLORER_URL ?? ''
const DRIP_AMOUNT  = 100

// ─── Subcomponents ───────────────────────────────────────────────────────────

function StatusDot({ online, state }: { online: boolean; state?: string }) {
  const active = online && (state === 'proposing' || state === 'full')
  return (
    <div className="flex items-center gap-2 text-sm">
      <span className={`w-2 h-2 rounded-full ${active ? 'bg-emerald-400 animate-pulse-slow' : online ? 'bg-amber-400 animate-pulse-slow' : 'bg-slate-600'}`} />
      <span className={active ? 'text-emerald-400' : online ? 'text-amber-400' : 'text-slate-500'}>
        {!online ? 'Offline' : state ?? 'Connecting…'}
      </span>
    </div>
  )
}

function TxHashDisplay({ hash }: { hash: string }) {
  const [copied, setCopied] = useState(false)
  const copy = () => {
    navigator.clipboard.writeText(hash)
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }
  const short = `${hash.slice(0, 8)}…${hash.slice(-8)}`
  const explorerHref = EXPLORER_URL ? `${EXPLORER_URL}/tx/${hash}` : null

  return (
    <div className="flex items-center gap-2 font-mono text-sm">
      {explorerHref ? (
        <a href={explorerHref} target="_blank" rel="noopener noreferrer"
           className="text-brand-400 hover:text-brand-300 underline underline-offset-2">
          {short}
        </a>
      ) : (
        <span className="text-slate-300">{short}</span>
      )}
      <button onClick={copy} className="text-slate-500 hover:text-slate-300 transition-colors" title="Copy full hash">
        {copied ? (
          <svg className="w-4 h-4 text-emerald-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
          </svg>
        ) : (
          <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2}
              d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z" />
          </svg>
        )}
      </button>
    </div>
  )
}

// ─── Main page ────────────────────────────────────────────────────────────────

export default function PortalPage() {
  const [currentTab, setCurrentTab] = useState<Tab>('faucet')

  // Faucet state (existing)
  const [address, setAddress]   = useState('')
  const [status, setStatus]     = useState<NetworkStatus>({ online: false })
  const [loading, setLoading]   = useState(false)
  const [result, setResult]     = useState<DripResult | null>(null)
  const [error, setError]       = useState<string | null>(null)
  const [cooldown, setCooldown] = useState<string | null>(null)

  // Wallet + Launch shared state
  const [loadedAddress, setLoadedAddress] = useState('')
  const [walletBalance, setWalletBalance] = useState<string | null>(null)
  const [walletLoading, setWalletLoading] = useState(false)

  // Launch Node form
  const [launchPayout, setLaunchPayout] = useState('')
  const [launchNodeName, setLaunchNodeName] = useState('mynode')
  const [copied, setCopied] = useState(false)

  // ── Poll network status every 10s ─────────────────────────────────────────
  const refreshStatus = useCallback(async () => {
    try {
      const r = await fetch('/api/status')
      const data = await r.json()
      setStatus(data)
    } catch {
      setStatus({ online: false })
    }
  }, [])

  useEffect(() => {
    refreshStatus()
    const id = setInterval(refreshStatus, 10_000)
    return () => clearInterval(id)
  }, [refreshStatus])

  // ── Cooldown countdown ────────────────────────────────────────────────────
  useEffect(() => {
    if (!cooldown) return
    const update = () => {
      const secs = Math.max(0, Math.floor((new Date(cooldown).getTime() - Date.now()) / 1000))
      if (secs <= 0) { setCooldown(null); return }
      const h = Math.floor(secs / 3600)
      const m = Math.floor((secs % 3600) / 60)
      const s = secs % 60
      setCooldown(
        h > 0 ? `${h}h ${m}m` : m > 0 ? `${m}m ${s}s` : `${s}s`
      )
    }
    update()
    const id = setInterval(update, 1000)
    return () => clearInterval(id)
  }, [cooldown])

  // ── Sync launch payout when user loads a wallet address ───────────────────
  useEffect(() => {
    if (loadedAddress && !launchPayout) {
      setLaunchPayout(loadedAddress)
    }
  }, [loadedAddress, launchPayout])

  // ── Wallet balance checker (direct RPC call from browser) ─────────────────
  const checkWalletBalance = async (addr: string) => {
    if (!addr.trim()) return
    setWalletLoading(true)
    setWalletBalance(null)
    try {
      const rpcUrls = [
        (process.env.NEXT_PUBLIC_RPC_URL || '').replace(/\/$/, ''),
        'http://127.0.0.1:6005',
        'http://127.0.0.1:5005',
      ].filter(Boolean)

      let balance: string | null = null
      for (const rpc of rpcUrls) {
        try {
          const res = await fetch(rpc, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
              method: 'account_info',
              params: [{ account: addr.trim(), ledger_index: 'validated' }],
            }),
          })
          if (res.ok) {
            const data = await res.json()
            const bal = data?.result?.account_data?.Balance
            if (bal) {
              balance = (parseInt(bal, 10) / 1_000_000).toFixed(2) + ' qXRP'
              break
            }
          }
        } catch {}
      }
      setWalletBalance(balance || 'Address not found or node unreachable')
    } catch {
      setWalletBalance('Unable to fetch balance (is the node public RPC reachable?)')
    } finally {
      setWalletLoading(false)
    }
  }

  // ── Generate the exact one-liner command ──────────────────────────────────
  const generateOneLiner = () => {
    const payout = launchPayout.trim() || 'rYOUR_WALLET_ADDRESS'
    const name = launchNodeName.trim() || 'mynode'
    return `curl -fsSL https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/bin/install/install-qxrp-validator.sh | bash -s -- \\
  --payout ${payout} \\
  --node-name ${name}`
  }

  const copyCommand = async () => {
    const cmd = generateOneLiner()
    try {
      await navigator.clipboard.writeText(cmd)
      setCopied(true)
      setTimeout(() => setCopied(false), 2200)
    } catch {
      prompt('Copy this one-liner:', cmd)
    }
  }

  // ── Jump from Wallet tab straight into Launch with address pre-filled ─────
  const openLaunchWithAddress = (addr: string) => {
    setLaunchPayout(addr)
    setLoadedAddress(addr)
    setCurrentTab('launch')
  }

  // ── Submit ────────────────────────────────────────────────────────────────
  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    setLoading(true)
    setError(null)
    setResult(null)

    try {
      const res = await fetch('/api/faucet', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ account: address.trim() }),
      })
      const data = await res.json()

      if (!res.ok) {
        setError(data.error ?? 'Request failed')
        if (data.reset) setCooldown(data.reset)
      } else {
        setResult(data)
        setAddress('')
      }
    } catch {
      setError('Network error. Please try again.')
    } finally {
      setLoading(false)
    }
  }

  // ─── Render ───────────────────────────────────────────────────────────────
  return (
    <div className="flex-1 flex flex-col min-h-screen">

      {/* Header with Tabs */}
      <header className="border-b border-slate-800/60 px-6 py-4">
        <div className="max-w-2xl mx-auto">
          <div className="flex items-center justify-between mb-3">
            <div className="flex items-center gap-3">
              <div className="w-8 h-8 rounded-lg bg-brand-500 flex items-center justify-center font-bold text-slate-950 text-sm">
                Q
              </div>
              <div>
                <div className="font-semibold text-white leading-tight">{NETWORK_NAME}</div>
                <div className="text-xs text-slate-500">Portal · Network {NETWORK_ID}</div>
              </div>
            </div>
            <StatusDot online={status.online} state={status.state} />
          </div>

          {/* Tab Navigation */}
          <div className="flex gap-1 rounded-xl bg-slate-900 p-1 border border-slate-800 w-fit">
            {[
              { id: 'faucet' as const, label: 'Faucet' },
              { id: 'wallet' as const, label: 'Wallet' },
              { id: 'launch' as const, label: 'Launch Node' },
            ].map((t) => (
              <button
                key={t.id}
                onClick={() => setCurrentTab(t.id)}
                className={`px-5 py-2 rounded-lg text-sm font-medium transition-all ${
                  currentTab === t.id
                    ? 'bg-brand-500 text-slate-950 shadow'
                    : 'text-slate-300 hover:text-white hover:bg-slate-800'
                }`}
              >
                {t.label}
              </button>
            ))}
          </div>
        </div>
      </header>

      {/* Main Content Area */}
      <main className="flex-1 px-4 py-10">
        <div className="max-w-2xl mx-auto">

          {/* ========== FAUCET TAB ========== */}
          {currentTab === 'faucet' && (
            <div className="space-y-6">
              <div className="text-center space-y-2">
                <h1 className="text-3xl font-bold text-white">
                  Get testnet <span className="text-brand-500">qXRP</span>
                </h1>
                <p className="text-slate-400 text-sm">
                  {DRIP_AMOUNT} qXRP per request · 24-hour cooldown per address
                </p>
              </div>

              <div className="card p-6 space-y-4 max-w-lg mx-auto">
                <form onSubmit={handleSubmit} className="space-y-4">
                  <div className="space-y-1.5">
                    <label htmlFor="address" className="block text-sm font-medium text-slate-300">
                      Your qXRP address
                    </label>
                    <input
                      id="address"
                      type="text"
                      value={address}
                      onChange={e => { setAddress(e.target.value); setError(null) }}
                      placeholder="r…"
                      autoComplete="off"
                      spellCheck={false}
                      className="input-field"
                      disabled={loading}
                    />
                  </div>

                  <button
                    type="submit"
                    disabled={loading || !address.trim() || !status.online}
                    className="btn-primary"
                  >
                    {loading ? (
                      <span className="flex items-center justify-center gap-2">
                        <svg className="w-4 h-4 animate-spin-slow" fill="none" viewBox="0 0 24 24">
                          <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
                          <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
                        </svg>
                        Sending…
                      </span>
                    ) : (
                      `Request ${DRIP_AMOUNT} qXRP`
                    )}
                  </button>
                </form>

                {error && (
                  <div className="rounded-xl bg-red-500/10 border border-red-500/20 px-4 py-3 text-sm text-red-400 space-y-1">
                    <div className="font-medium">{error}</div>
                    {typeof cooldown === 'string' && cooldown.length > 0 && cooldown !== 'Invalid Date' && (
                      <div className="text-red-400/70">Try again in {cooldown}</div>
                    )}
                  </div>
                )}

                {result && (
                  <div className="rounded-xl bg-emerald-500/10 border border-emerald-500/20 px-4 py-4 space-y-3">
                    <div className="flex items-center gap-2 text-emerald-400 font-medium text-sm">
                      <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
                      </svg>
                      {result.amount} qXRP sent!
                    </div>
                    <div className="grid grid-cols-[auto_1fr] gap-x-3 gap-y-1.5 text-sm">
                      <span className="text-slate-500">To</span>
                      <span className="font-mono text-slate-300 text-xs break-all">{result.account}</span>
                      <span className="text-slate-500">Tx</span>
                      <TxHashDisplay hash={result.txHash} />
                    </div>
                  </div>
                )}
              </div>

              <div className="grid grid-cols-2 gap-3 max-w-lg mx-auto">
                {[
                  { label: 'Ledger', value: status.ledger?.toLocaleString() ?? '—' },
                  { label: 'Peers', value: status.peers?.toString() ?? '—' },
                  { label: 'State', value: status.state ?? '—' },
                  { label: 'Load factor', value: status.loadFactor?.toFixed(2) ?? '—' },
                ].map(({ label, value }) => (
                  <div key={label} className="card px-4 py-3">
                    <div className="text-xs text-slate-500 mb-0.5">{label}</div>
                    <div className="font-mono text-sm text-slate-200">{value}</div>
                  </div>
                ))}
              </div>

              <p className="text-center text-xs text-slate-600 max-w-lg mx-auto">
                For testing only ·{' '}
                <a href="https://github.com/beartec-jpg/qXRP" target="_blank" rel="noopener noreferrer"
                   className="text-slate-500 hover:text-slate-400 underline underline-offset-2">
                  qXRP on GitHub
                </a>
              </p>
            </div>
          )}

          {/* ========== WALLET TAB ========== */}
          {currentTab === 'wallet' && (
            <div className="max-w-lg mx-auto space-y-6">
              <div className="text-center">
                <h2 className="text-2xl font-semibold">Wallet</h2>
                <p className="text-slate-400 text-sm mt-1">Load any address to view balance and launch a validator</p>
              </div>

              <div className="card p-6 space-y-4">
                <div>
                  <label className="block text-sm font-medium text-slate-300 mb-1.5">Address</label>
                  <input
                    value={loadedAddress}
                    onChange={(e) => setLoadedAddress(e.target.value)}
                    placeholder="rYourAddress..."
                    className="input-field"
                  />
                </div>

                <div className="flex gap-3">
                  <button
                    onClick={() => checkWalletBalance(loadedAddress)}
                    disabled={!loadedAddress.trim() || walletLoading}
                    className="btn-primary flex-1"
                  >
                    {walletLoading ? 'Checking…' : 'Check Balance'}
                  </button>
                  <button
                    onClick={() => openLaunchWithAddress(loadedAddress)}
                    disabled={!loadedAddress.trim()}
                    className="flex-1 rounded-xl border border-brand-500/60 text-brand-400 hover:bg-brand-500/10 py-3.5 font-semibold transition"
                  >
                    Open Node with this address →
                  </button>
                </div>

                {walletBalance && (
                  <div className="rounded-xl bg-slate-800 border border-slate-700 px-4 py-3 font-mono text-sm">
                    Balance: <span className="text-emerald-400">{walletBalance}</span>
                  </div>
                )}
              </div>

              <div className="card p-5 text-sm text-slate-300">
                <div className="font-medium text-slate-200 mb-2">Tip</div>
                After you run the one-liner from the <span className="text-brand-400">Launch Node</span> tab,
                come back here, paste the validator account it printed, and check its balance.
                That’s where rewards will accumulate.
              </div>
            </div>
          )}

          {/* ========== LAUNCH NODE TAB (the important one) ========== */}
          {currentTab === 'launch' && (
            <div className="max-w-xl mx-auto space-y-6">
              <div className="text-center">
                <h2 className="text-2xl font-semibold">Launch a Validator Node</h2>
                <p className="text-slate-400 mt-1">One command. Auto-bonds after you fund it.</p>
              </div>

              {/* THE EXACT WARNING THE USER REQUESTED */}
              <div className="rounded-2xl border border-amber-500/40 bg-amber-500/10 px-5 py-4 text-amber-200">
                <div className="flex gap-3">
                  <div className="text-xl mt-0.5">⚠️</div>
                  <div className="text-sm leading-snug">
                    <span className="font-semibold">You need 1,000 qXRP to bond.</span><br />
                    It is <span className="underline">recommended to get that first</span> from the faucet or your loaded wallet before running the command.
                  </div>
                </div>
              </div>

              <div className="card p-6 space-y-5">
                <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
                  <div>
                    <label className="block text-sm font-medium text-slate-300 mb-1.5">Your payout / main wallet address</label>
                    <input
                      value={launchPayout}
                      onChange={(e) => setLaunchPayout(e.target.value)}
                      placeholder="rYourWallet..."
                      className="input-field"
                    />
                    <p className="text-[11px] text-slate-500 mt-1">Rewards will be easy to send here later.</p>
                  </div>
                  <div>
                    <label className="block text-sm font-medium text-slate-300 mb-1.5">Node name</label>
                    <input
                      value={launchNodeName}
                      onChange={(e) => setLaunchNodeName(e.target.value)}
                      className="input-field"
                    />
                  </div>
                </div>

                {/* Live command preview */}
                <div>
                  <div className="flex items-center justify-between mb-2">
                    <div className="text-sm font-medium text-slate-300">Your one-liner (copy &amp; paste this)</div>
                    <button
                      onClick={copyCommand}
                      className="text-xs px-3 py-1 rounded-lg bg-slate-800 hover:bg-slate-700 border border-slate-700 flex items-center gap-1.5"
                    >
                      {copied ? (
                        <>Copied ✓</>
                      ) : (
                        <>Copy command</>
                      )}
                    </button>
                  </div>
                  <pre className="bg-slate-950 border border-slate-800 rounded-xl p-4 text-[13px] font-mono text-emerald-300 overflow-x-auto whitespace-pre-wrap">
{generateOneLiner()}
                  </pre>
                </div>

                <div className="text-xs text-slate-400 leading-relaxed pt-2 border-t border-slate-800">
                  Run this on Ubuntu 22.04/24.04. The command will print a <span className="font-mono text-slate-200">validator account</span> (r...). 
                  Send it ≥1,100 qXRP from the wallet above. It will auto-detect the funds, register + bond the node, and set up reward claiming.
                </div>
              </div>

              <div className="text-center text-xs text-slate-500">
                Full instructions + troubleshooting → <a href="https://github.com/beartec-jpg/qXRP/blob/develop/docs/validator-onboarding.md" target="_blank" className="underline">docs/validator-onboarding.md</a>
              </div>
            </div>
          )}

        </div>
      </main>

      {/* Footer */}
      <footer className="border-t border-slate-800/60 py-4 text-center text-[11px] text-slate-600">
        qXRP Testnet · Not real value ·{' '}
        <a href="https://github.com/beartec-jpg/qXRP" target="_blank" className="hover:text-slate-400 underline underline-offset-2">GitHub</a>
      </footer>
    </div>
  )
}
