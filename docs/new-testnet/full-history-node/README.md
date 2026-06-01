# Running a qXRP Full History Node (Clean New Testnet)

This guide is for running a proper **Full History Node** on the new clean qXRP testnet.

> **Important**: Full history nodes require significant resources. 61 GB of free disk is the **bare minimum** to get started. You will eventually need to expand storage.

## Quick Start (Docker - Recommended)

1. **Clone the repository** (if you haven't already):

```bash
git clone https://github.com/beartec-jpg/qXRP.git
cd qXRP/docs/new-testnet/full-history-node
```

2. **Create the data directory** (recommended location):

```bash
sudo mkdir -p /var/lib/qxrp
sudo chown $USER:$USER /var/lib/qxrp
```

3. **Start the node**:

```bash
docker compose up -d
```

4. **Check that it's running**:

```bash
docker logs -f qxrp-full-history
```

5. **Verify it's synced** (wait a few minutes then run):

```bash
curl -s http://localhost:6005 | python3 -m json.tool | grep -E '"server_state"|"complete_ledgers"'
```

## What This Setup Includes

- Network ID: **1001** (new clean testnet)
- Full ledger history (`[ledger_history] full`)
- Large node size for better performance
- Proper Docker resource limits (8GB memory limit)
- Log rotation
- Healthcheck
- Public RPC on port 6005
- Peer port on 51235

## Important Resource Requirements

| Resource     | Minimum     | Recommended     | Notes |
|--------------|-------------|------------------|-------|
| **Disk**     | 60 GB       | 200 GB+          | Full history grows over time |
| **RAM**      | 4 GB        | 8 GB+            | More is better |
| **CPU**      | 2 cores     | 4+ cores         | Helps during initial sync |

## Common Commands

```bash
# View logs
docker logs -f qxrp-full-history

# Restart node
docker compose restart

# Stop node
docker compose down

# Update to latest version
docker compose pull
docker compose up -d
```

## Making the RPC Public (Optional)

If you want to expose the public RPC (port 6005) to the internet:

- Make sure your firewall only allows trusted IPs if possible
- Consider putting it behind a reverse proxy with rate limiting for production use

## Next Steps After Starting

1. Wait for the node to fully sync (can take many hours for full history).
2. Once synced, you can use it for:
   - Running a public explorer
   - Powering a faucet
   - Operating a validator (with separate validator keys)
   - General infrastructure

## Security Notes

- The admin RPC (port 5005) is bound to localhost only — do not expose it.
- Keep your server updated.
- Monitor disk usage regularly.

## Questions?

See the main [NEW_TESTNET_BOOTSTRAP.md](../../NEW_TESTNET_BOOTSTRAP.md) for the overall new testnet strategy.