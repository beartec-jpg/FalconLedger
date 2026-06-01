# Running a qXRP Validator (Clean New Testnet)

This setup is for running a **Validator** on the new clean qXRP testnet (Network ID 1001).

Validators do **not** need full history. This setup is lighter than a full history node.

## Quick Start

```bash
git clone https://github.com/beartec-jpg/qXRP.git
cd qXRP/docs/new-testnet/validator

sudo mkdir -p /var/lib/qxrp-validator
sudo chown $USER:$USER /var/lib/qxrp-validator

docker compose up -d
```

## Important Notes

- You still need to generate validator keys separately.
- You must fund your validator with at least 1,000 qXRP to bond.
- The admin RPC (port 5005) is bound to localhost only — do not expose it.

## Common Commands

```bash
# View logs
docker logs -f qxrp-validator

# Restart
docker compose restart

# Stop
docker compose down
```

See the main [NEW_TESTNET_BOOTSTRAP.md](../../NEW_TESTNET_BOOTSTRAP.md) for the overall process of becoming a validator on the new testnet.