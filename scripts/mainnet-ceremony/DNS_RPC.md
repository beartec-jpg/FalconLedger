# Public RPC / DNS (fill before T0)

Reserve hostnames **now**; point DNS only at T0 so scanners do not hit empty boxes early.

| Record | Purpose | Value (fill) | TTL at T0 |
|--------|---------|--------------|-----------|
| `rpc.mainnet.example` | Public JSON-RPC / WS | LB → fullhistory or dedicated RPC | 60s → 300s |
| `ws.mainnet.example` | Public WebSocket | same or split | 60s |
| `status.mainnet.example` | Optional status page | static / grafana | 300s |

## Capacity guidance

- Do **not** expose admin port `5005` publicly.
- Public RPC: rate-limit + separate from validators if possible.
- Validators: peer `51235` only; admin localhost.

## Publish at T0

```text
Network name:     Falcon Ledger
Network ID:       1026
Public RPC:       https://rpc.mainnet.example
Image digest:     (from IMAGE_DIGEST.txt)
Install:          QXRP_XRPLD_IMAGE=<digest> bash install-qxrp-validator.sh …
Faucet drip:      100 FALCON, 5/day, 1h cooldown
Airdrop:          mainnet activity only, 60 days, 2B pool
```
