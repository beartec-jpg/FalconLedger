export interface FalconWallet {
  address: string
  balance: string
  publicKey: string
  label: string
}

export interface Transaction {
  hash: string
  type: string
  from: string
  to: string
  amount: string
  fee: string
  ledgerIndex: number
  timestamp: string
  status: 'validated' | 'pending' | 'failed'
}

export interface AgentRule {
  id: string
  name: string
  type: 'watch' | 'schedule' | 'condition'
  condition: string
  action: string
  enabled: boolean
  lastTriggered?: string
}

export interface AgentAction {
  id: string
  ruleId: string
  timestamp: string
  description: string
  txHash?: string
  status: 'pending' | 'executed' | 'failed'
}

export interface AgentStatus {
  running: boolean
  activeRules: number
  executedToday: number
  lastAction?: AgentAction
}
