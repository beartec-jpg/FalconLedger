# Falcon Ledger Agent Wallet

Falcon Ledger Agent Wallet is an agent-first wallet for the Falcon Ledger quantum-resistant blockchain.

This project is designed as a standalone, licensable wallet product that can be deployed by any third party running Falcon Ledger infrastructure.

## Vision

The Falcon Ledger daemon and this wallet are separate products:

- **Falcon Ledger daemon**: core blockchain infrastructure
- **Falcon Ledger Agent Wallet**: deployable wallet and automation layer built on top of Falcon Ledger

This separation enables operators to run their own wallet infrastructure for users while leveraging Falcon Ledger and qXRP.

## Product Modes

### 1) Standard Wallet

- qXRP balance dashboard
- Send and receive flows
- Transaction history feed
- Falcon key-aware wallet context (Falcon-512)

### 2) Agent Mode

- Autonomous rules engine
- Natural language command input for payment automation
- Live agent action feed
- Rule management (watch, schedule, condition)
- Agent activity and automation statistics

## Falcon Ledger Connection

The wallet connects to Falcon Ledger-compatible endpoints:

- **WebSocket** for subscriptions and live account events
- **JSON-RPC** for account queries and transaction submission

Falcon Ledger is XRPL API-compatible, so the `xrpl` package is used for connectivity.

> Falcon-512 public keys use an `0xFB` prefix byte and are 898 bytes in Falcon Ledger key material flows.

## Getting Started

```bash
pnpm install
pnpm dev
```

Open `http://localhost:3000`.

## Environment Variables

| Variable | Required | Default | Description |
|---|---:|---|---|
| `NEXT_PUBLIC_FALCON_NODE_WS_URL` | No | `ws://localhost:6006` | Public WebSocket endpoint used by wallet views |
| `FALCON_NODE_RPC_URL` | No | `http://localhost:5005` | Server-side Falcon Ledger RPC URL |
| `AGENT_POLL_INTERVAL_MS` | No | `10000` | Polling interval for agent status UI |
| `OPENAI_API_KEY` | No | unset | Optional future LLM bridge key |

## Architecture Overview

- `src/app/wallet/page.tsx`: Standard wallet dashboard
- `src/app/agent/page.tsx`: Agent command center (hero page)
- `src/app/api/wallet/*`: Wallet APIs (balance, send, history)
- `src/app/api/agent/*`: Agent APIs (status, command, rules)
- `src/lib/falcon-client.ts`: Falcon Ledger WebSocket client wrapper
- `src/lib/agent-engine.ts`: In-memory rule engine and action execution

## Agent Rules System

Rules are modeled as:

- **watch**: trigger based on observed account/network activity
- **schedule**: trigger on recurring time expressions
- **condition**: trigger when wallet/account conditions are met

The prototype executes rules through an event/callback pattern and records actions with status (`pending`, `executed`, `failed`) plus optional transaction hash.

## Standalone Product Positioning

This wallet is intended to be sold or licensed as a separate product from the Falcon Ledger daemon. Operators can deploy it independently as wallet infrastructure for qXRP users.

## License

MIT
