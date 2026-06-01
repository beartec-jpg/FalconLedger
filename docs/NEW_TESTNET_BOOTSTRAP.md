# qXRP New Testnet Bootstrap Guide (Clean Launch)

**Purpose**: This document describes the recommended process for launching a fresh, clean qXRP testnet after the May 2026 security incident.

## Goals for the Clean Launch

- Fresh Network ID (1001)
- New dedicated faucet account (never reuse the old genesis account)
- Bounded bootstrap supply
- All secrets kept out of git history
- Same economic and incentive model as the previous testnet (no functional changes)
- Clean, auditable starting state

## High-Level Bootstrap Process

1. **Generate New Genesis / Bootstrap Keys**
   - Create a fresh bootstrap account (this will hold the initial supply).
   - Do **not** use the old genesis secret (`snoPBrXtMeMyMHUVTgbuqAfg1SUTb`).

2. **Define Supply & Allocation**
   - Total supply: 200,000,000,000 qXRP (same as before)
   - Genesis allocation: Small percentage for liquidity, development, emergency reserve
   - Treasury: Majority of supply (protocol-controlled)

3. **Create a Dedicated Faucet Account**
   - Generate a new account specifically for the faucet.
   - Fund it with a bounded amount from the bootstrap supply (e.g. 5–10 million qXRP initially).
   - This account will be used by the public faucet(s).
   - The faucet should be refilled periodically from the treasury/bootstrap as needed.

4. **Deploy Initial Nodes**
   - Use the deployment scripts with the new Network ID (`1001`).
   - Configure validators with fresh keys.
   - Bring up the initial validator set.

5. **Issue Fresh Stablecoins (Optional but Recommended)**
   - Re-issue QUSDC (QUC) and QUSDT (QUT) on the new network with new issuers.
   - Update all `.env.example` files and documentation with the new issuer addresses.

6. **Configure and Deploy Faucet(s)**
   - Use the `qXRP-faucet-wallet` repo (or `tools/faucet-web`).
   - Set `FAUCET_ACCOUNT` and `FAUCET_SECRET` via environment variables only (Vercel + local `.env.local`).
   - Never commit real secrets.

7. **Update All Public Documentation**
   - Update `docs/DeploymentGuide.md` with new network details.
   - Update READMEs in both repos.
   - Publish the new Network ID, RPC endpoints, and faucet information.

8. **Announce the New Testnet**
   - Clearly communicate that the previous testnet (Network ID 999) is deprecated.
   - Provide migration instructions if needed (most users will just need new accounts + new faucet).

## Recommended Account Strategy

- **Bootstrap Account**: Holds the full initial supply. Used only for initial distribution.
- **Faucet Account**: Separate, limited-balance account. This is the only account the public faucet uses.
- **Validator Payout Accounts**: Separate accounts for each validator (for reward claiming).

## Security Rules (Non-Negotiable)

- Never commit `.env*` files containing real secrets (except `.env.example` with placeholders).
- All production secrets live only in Vercel environment variables.
- The faucet account must have a bounded balance and be refilled manually or via controlled process.
- Rotate faucet secret periodically if the account is heavily used.

## Files That Should Use the New Network ID

- All deployment and installation scripts (already updated to default to 1001)
- `.env.example` files in `qXRP-faucet-wallet` and `qXRP/tools/faucet-web`
- `docs/DeploymentGuide.md`
- Any new public documentation

## Post-Launch Checklist

- [ ] New Network ID confirmed live (1001 or chosen value)
- [ ] Dedicated faucet account created and funded
- [ ] Faucet deployed with secrets only in Vercel
- [ ] Stablecoins re-issued (if desired)
- [ ] All public docs updated
- [ ] Old testnet materials archived
- [ ] Git history cleaned of old secrets (in both repos)
- [ ] Old testnet nodes can be decommissioned

---

**Last Updated**: 2026-05-30  
This document should be updated after the actual new testnet launch with the final values used.