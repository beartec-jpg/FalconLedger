# One-Command Validator Onboarding (Testnet)

This guide shows the **fastest way** to go from zero to a bonded, reward-earning qXRP validator using a single terminal command.

> **Target audience**: Developers, operators, and anyone who wants to participate in Proof-of-Participation rewards on the qXRP testnet with minimal friction.

---

## Prerequisites

- Ubuntu 22.04 / 24.04 or Debian 12 (other distros may work with Docker)
- At least 4 GB RAM + 80 GB disk (more is better)
- A machine with a public IP and port 51235 reachable from the internet (for real validator operation)
- A funded qXRP address with **≥ 1,100 qXRP** (use the faucet)

---

## The Magic One-Liner (Recommended Flow)

1. Go to the official **qXRP Web Portal** (faucet + wallet + node launcher):
   - https://github.com/beartec-jpg/qXRP-faucet-wallet (the dedicated web app repo — this is the site users actually visit)

2. Get testnet qXRP from the faucet.

3. Load (or create) your address in the **Wallet**.

4. Click the **node icon** (or "Run a Validator Node") in the wallet dashboard.

5. You will see the required warning:
   > **You need 1,000 qXRP to bond.** It is recommended to get that first from the faucet before running the command.

6. Your loaded wallet address is already filled in as the `--payout` destination.

7. Copy the big green **"Copy one-liner command"** button (your wallet is already set as `--payout`) and paste it on your server/VPS:

```bash
curl -fsSL https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/bin/install/install-qxrp-validator.sh | bash -s -- \
  --payout rYOUR_MAIN_WALLET_ADDRESS \
  --node-name mynode
```

That is the entire onboarding experience.

---

## What the Command Actually Does

1. Preflight checks (RAM, disk, Docker).
2. Installs Docker if missing.
3. **Generates your validator identity**:
   - Classical secp256k1 validation seed (for consensus + bonding)
   - Synthetic but protocol-valid Falcon-512 identity pubkey (accepted today; real keys coming soon)
4. Writes hardened production config (`~/.qxrp/config/xrpld.cfg`) with `ProofOfParticipation` enabled.
5. Starts the official `qxrp/xrpld` Docker container as a validator.
6. **Prints in huge text the exact account you must fund** (`r...`).
7. If you passed `--payout`, it remembers it.
8. **Polls your validator account every 15 seconds**.
9. As soon as it sees ≥ ~1,100 qXRP, it **automatically** submits:
   - `ValidatorRegister`
   - `ValidatorBond` (1,000 qXRP)
10. Installs a lightweight reward claimer (`~/.qxrp/qxrp-claimer.py`) + a cron job.
11. Shows you the final dashboard URL, peer port, and next steps in the portal.

After this you will see your validator in "proposing" state and it will start accumulating composite score and rewards after the next epoch.

---

## After the Command Finishes

### 1. Fund the printed validator account

The script will have printed something like:

```
YOUR VALIDATOR ACCOUNT (FUND THIS NOW)
rAbCdEfGhIjK...   ← send 1,100–1,200 qXRP here
```

Send from the same wallet address you loaded in the portal.

### 2. Watch auto-bonding

The script keeps running the funding detection loop. Once funded it will automatically register + bond.

You can also watch progress with:

```bash
docker logs -f qxrp_validator
```

### 3. Check status in the Portal

Go back to the Wallet / Validators section of the portal. It should now show your validator as **BONDED** with a composite score.

### 4. Claiming rewards

Rewards are claimed automatically by the cron-installed claimer (every 30 minutes) when your composite score is high enough (≥ 500 bps).

You can also run it manually anytime:

```bash
~/.qxrp/qxrp-claimer.py --keys ~/.qxrp/keys/validator-keys.json
```

Rewards land in the validator account (`r...` printed during setup).

### 5. Withdrawing to your main wallet

Use the **Wallet** tab in the portal:

- Load the validator account (or import its seed from the backup file if you want full control).
- Send excess qXRP (above ~300 qXRP safety reserve) to your main payout address.

A future version of the portal will have a one-click "Claim + Withdraw to my payout address" button.

---

## Important Files (Backup These!)

All stored under `~/.qxrp/`:

| File | Purpose | Sensitivity |
|------|---------|-------------|
| `keys/validator-keys.json` | All seeds + pubkeys + payout address | **EXTREMELY SECRET** |
| `keys/seed.txt` | Just the classical validation seed | **EXTREMELY SECRET** |
| `config/xrpld.cfg` | Node config (contains the seed) | Secret |
| `config/validators.txt` | Your UNL | Can be public |
| `qxrp-claimer.py` | Reward claim helper | Contains no secrets |

**Losing the keys/seed = permanent loss of this validator identity and any unclaimed rewards.**

Store `validator-keys.json` offline in multiple safe places.

---

## Common Next Steps & Troubleshooting

### Open peer port

On cloud providers (Hetzner, AWS, etc.) you **must** open TCP port 51235 inbound.

Example (UFW):

```bash
sudo ufw allow 51235/tcp comment "qXRP validator peer"
```

### Node stays in "tracking" for a long time

Normal on first start with `full` history. It can take hours. Check:

```bash
docker logs qxrp_validator | tail -100
```

Look for `server_state`.

### "tecDUPLICATE" or "tecNO_PERMISSION" on bonding

The script is idempotent. If you see these it usually means the validator is already registered/bonded — success.

Verify with the portal or:

```bash
curl -s http://127.0.0.1:5005 -H 'Content-Type: application/json' \
  -d '{"method":"ledger_entry","params":[{"validator_bond":{"account":"rYOUR_VALIDATOR_ACCOUNT"},"ledger_index":"validated"}]}'
```

### How do I rotate to real Falcon keys later?

When we add proper `falcon_create` RPC + key rotation support, you will be able to submit a new `ValidatorRegister` with a real post-quantum key. The synthetic keys used by the installer today are only a bootstrap convenience.

### Switch from Docker to native systemd later

The advanced production path is still `bin/install/deploy-cloud-validator.sh`. It gives you better performance and a proper systemd service. The Docker path is intentionally the easiest on-ramp.

---

## For Operators Who Want the Old Simple Docker Path

If you just want a plain tracking node (no keys, no bonding, no rewards):

```bash
# Old behavior (still supported)
curl -fsSL https://raw.githubusercontent.com/beartec-jpg/qXRP/develop/bin/install/install-qxrp-validator.sh | bash
```

But you are strongly encouraged to use the new `--payout` flow instead.

---

## Feedback & Contributing

This onboarding flow is brand new. Please open issues or PRs with:

- Success/failure reports on specific VPS providers
- UX friction points
- Ideas for the portal wallet integration

See the main [README](../README.md) and [DeploymentGuide](DeploymentGuide.md) for more infrastructure patterns (monitoring, UNL publisher, etc.).

---

**You are now running a real qXRP validator. Welcome to the network.**