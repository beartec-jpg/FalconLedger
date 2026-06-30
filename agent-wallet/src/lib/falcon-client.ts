import { Client } from 'xrpl'
import { FALCON_NODE_WS_URL } from '@/lib/constants'
import type { Transaction } from '@/lib/types'

type AccountCallback = (tx: Transaction) => void

export class FalconLedgerClient {
  private client: Client | null = null
  private connecting: Promise<Client> | null = null

  private async getClient(): Promise<Client> {
    if (this.client?.isConnected()) {
      return this.client
    }

    if (this.connecting) {
      return this.connecting
    }

    this.connecting = (async () => {
      const nextClient = new Client(FALCON_NODE_WS_URL)
      await nextClient.connect()
      this.client = nextClient
      return nextClient
    })()

    try {
      return await this.connecting
    } finally {
      this.connecting = null
    }
  }

  async getBalance(address: string): Promise<string> {
    const client = await this.getClient()
    const response = await client.request({
      command: 'account_info',
      account: address,
      ledger_index: 'validated'
    })

    return response.result.account_data.Balance
  }

  async getAccountTransactions(address: string): Promise<Transaction[]> {
    const client = await this.getClient()
    const response = await client.request({
      command: 'account_tx',
      account: address,
      limit: 10,
      ledger_index_min: -1,
      ledger_index_max: -1
    })

    return (response.result.transactions || []).map((entry: any) => {
      const tx = entry.tx || {}
      return {
        hash: tx.hash || 'unknown',
        type: tx.TransactionType || 'Unknown',
        from: tx.Account || 'unknown',
        to: tx.Destination || 'unknown',
        amount: tx.Amount?.toString?.() || '0',
        fee: tx.Fee?.toString?.() || '0',
        ledgerIndex: entry.ledger_index || 0,
        timestamp: new Date().toISOString(),
        status: entry.validated ? 'validated' : 'pending'
      } as Transaction
    })
  }

  async submitTransaction(txBlob: string) {
    const client = await this.getClient()
    return client.request({
      command: 'submit',
      tx_blob: txBlob
    })
  }

  async subscribeToAccount(address: string, callback: AccountCallback): Promise<() => void> {
    const client = await this.getClient()
    const listener = (event: any) => {
      const tx = event?.transaction
      if (!tx) {
        return
      }

      if (tx.Account !== address && tx.Destination !== address) {
        return
      }

      callback({
        hash: tx.hash || `pending-${Date.now()}`,
        type: tx.TransactionType || 'Unknown',
        from: tx.Account || 'unknown',
        to: tx.Destination || 'unknown',
        amount: tx.Amount?.toString?.() || '0',
        fee: tx.Fee?.toString?.() || '0',
        ledgerIndex: event.ledger_index || 0,
        timestamp: new Date().toISOString(),
        status: event.validated ? 'validated' : 'pending'
      })
    }

    client.on('transaction', listener)
    await client.request({ command: 'subscribe', accounts: [address] })

    return async () => {
      client.off('transaction', listener)
      await client.request({ command: 'unsubscribe', accounts: [address] })
    }
  }
}

export const falconClient = new FalconLedgerClient()
