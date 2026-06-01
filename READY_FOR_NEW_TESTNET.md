# READY FOR NEW TESTNET WIPE & REBUILD

**Date**: 2026-05-30  
**Status**: ✅ **READY TO WIPE AND REBUILD** (High-priority wallet security items from audit also addressed)

---

## Summary

All preparatory code, documentation, and repository hygiene work for a clean new qXRP testnet has been completed.

We are now in a good position to:
- Shut down the old testnet (Network ID 999)
- Launch a fresh testnet (recommended Network ID: **1001**)
- Use a dedicated faucet account with bounded funds
- Keep all secrets properly out of git

---

## What Has Been Completed

### 1. Code & Config Cleanup (No Functional Changes)
- Default Network ID changed from 999 → **1001** across:
  - All deployment and installation scripts
  - Faucet applications (both repos)
  - Documentation
- All references to the old genesis account (`rHb9CJAWyB4rj91VRWn96DkukG4bwdtyTh`) removed or clearly marked as deprecated in active files.
- `.env.example` files sanitized in both `qXRP-faucet-wallet` and `qXRP/tools/faucet-web`.
- `.env.production` removed from `qXRP-faucet-wallet`.

### 2. History Purging (Critical Security Step)
- Used `git filter-repo` to remove sensitive `.env` files (including those containing the old genesis secret) from git history in **both repositories**.
- Old secret `snoPBrXtMeMyMHUVTgbuqAfg1SUTb` should no longer be discoverable in current history.

**Note**: Remotes were removed during filtering (standard behavior). They have been re-added locally.

### 3. Documentation & Guidance
- Created `docs/NEW_TESTNET_BOOTSTRAP.md` — clear guide for the clean launch using the same economic model.
- Updated `docs/DeploymentGuide.md` with new network ID and bootstrap notes.
- Updated main `README.md` with links to both audit reports and the new bootstrap guide.
- Created `archive/old-testnet-2026-05/` containing:
  - Old chain report
  - All destructive testing materials and results
- Created comprehensive `docs/security/security-testing.md`.

### 4. Repository Hygiene
- Old testnet materials moved to `archive/`.
- All active scripts and docs now default to or recommend the new Network ID (1001).

---

## High-Priority Wallet / Faucet Security Fixes Addressed (from separate audit)

Before the new testnet launch, the following high-priority issues were also fixed in `qXRP-faucet-wallet`:

- **H-1 (Seed leaves device)**: Updated misleading "never leaves device" language in the wallet UI and added clear warnings that the seed is sent to the server-side signing proxy for Falcon field injection. Strong disclosure added that this wallet should not be used with real funds.
- **H-2 (Plaintext HTTP)**: Updated RPC examples and documentation to require https:// in production. Plain HTTP nodes are no longer presented as the default.
- **Rate limiting**: Modified `rate-limit.ts` to **fail closed** in production if no Upstash/Redis is configured (prevents silent unlimited faucet abuse on Vercel). In-memory fallback is now dev-only.

These changes make the new clean testnet significantly safer to expose publicly.

## What Still Needs to Happen (Outside This Environment)

1. **Create New Dedicated Faucet Account**
   - Generate fresh keys on the new testnet.
   - Fund it with a bounded amount from the new bootstrap supply.

2. **Deploy Fresh Infrastructure**
   - New nodes with Network ID 1001.
   - Fresh validator keys (recommended).
   - Re-issue stablecoins (QUC / QUT) if desired.

3. **Update Vercel Environment Variables**
   - Set new `FAUCET_ACCOUNT` and `FAUCET_SECRET` for the faucet deployment(s).
   - Set correct `XRPLD_RPC_URL` and `NEXT_PUBLIC_NETWORK_ID`.

4. **Force Push History Rewrite** (after local verification)
   - Both repos will require `--force` push because history was rewritten.
   - Coordinate with any other collaborators.

5. **Announce the New Testnet**
   - Update any external documentation, Discord, etc.
   - Clearly state that the old testnet (ID 999) is deprecated.

---

## Repos Status

| Repo                        | Branch   | Last Cleanup Commit                  | History Purged? | Ready? |
|----------------------------|----------|--------------------------------------|------------------|--------|
| `qXRP`                     | develop  | `2fb1e6835`                         | Yes             | Yes    |
| `qXRP-faucet-wallet`       | main     | `51006df`                           | Yes             | Yes    |

---

## Final Recommendation

You are now ready to:

1. Shut down the old nodes (Network ID 999).
2. Launch the new clean testnet with Network ID **1001**.
3. Use a single dedicated faucet account with limited funds.
4. Keep all secrets exclusively in Vercel + local `.env.local`.

Once the new testnet is live and verified, you can force-push the cleaned history to both GitHub repositories.

---

**Prepared by**: Automated remediation session — 2026-05-30  
**Next Manual Step**: Create new faucet account + deploy fresh nodes with ID 1001.