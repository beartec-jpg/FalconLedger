import type { Metadata } from 'next'
import './globals.css'

const NETWORK = process.env.NEXT_PUBLIC_NETWORK_NAME ?? 'qXRP Testnet'

export const metadata: Metadata = {
  title: `${NETWORK} Faucet`,
  description: `Get free ${NETWORK} tokens for development and testing.`,
  icons: { icon: '/favicon.svg' },
}

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" className="dark">
      <body className="min-h-screen flex flex-col">{children}</body>
    </html>
  )
}
