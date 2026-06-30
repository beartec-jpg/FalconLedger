import { BalanceCard } from '@/components/wallet/BalanceCard'
import { SendPayment } from '@/components/wallet/SendPayment'
import { ReceiveCard } from '@/components/wallet/ReceiveCard'
import { TransactionFeed } from '@/components/wallet/TransactionFeed'

const DEMO_ADDRESS = 'rFalconWalletDemo11111111111111111111'

export default function WalletPage() {
  return (
    <div className="space-y-4">
      <h1 className="text-2xl font-semibold text-white">Wallet Dashboard</h1>
      <BalanceCard address={DEMO_ADDRESS} />
      <div className="grid gap-4 md:grid-cols-2">
        <SendPayment />
        <ReceiveCard address={DEMO_ADDRESS} />
      </div>
      <TransactionFeed address={DEMO_ADDRESS} />
    </div>
  )
}
