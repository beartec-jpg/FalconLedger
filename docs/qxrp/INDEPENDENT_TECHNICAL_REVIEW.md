# Independent Third-Party Technical Review  
## Falcon Ledger (Protocol) and Falcon Ledger Web Portal

| Field | Detail |
|-------|--------|
| **Projects** | [FalconLedger](https://github.com/beartec-jpg/FalconLedger) · [Falcon-faucet-wallet](https://github.com/beartec-jpg/Falcon-faucet-wallet) |
| **Scope** | Architecture, tokenomics, security posture, product surface, mainnet readiness |
| **Network under review** | Falcon Ledger **testnet** (network ID **1001**); planned mainnet network ID **1026** |
| **Public portal** | [falcon-ledger.com](https://falcon-ledger.com) |
| **Review date** | 23 July 2026 |
| **Classification** | Pre-mainnet / testnet — not a formal penetration test or financial audit |
| **Method** | Direct repository review (source, constants, transactors, contracts, portal APIs, public docs) |

---

## 1. Executive summary

**Falcon Ledger** is a quantum-resistant fork of the XRP Ledger reference software. It keeps RPCA consensus and low-cost finality, and replaces classical signature defaults with **Falcon-512** for accounts, validations, and consensus participation. On top of that base it adds a **fixed 200 billion** native token supply (**qXRP** / UI label **FALCON**), a **keyless protocol treasury holding 98% of supply**, epoch-based emission (CID), validator bonding and on-ledger scoring, pull-based rewards, limited slashing, bounded on-chain governance, optional account names, native DEX/AMM, and a lending/vault stack.

The companion **web portal** is a full testnet product: faucet, passkey-secured Falcon wallet, swap, pool, lend, Sepolia USDC bridge, explorer, rewards/validator UX, community board, and related tooling. User keys are generated and signed **client-side** (WASM); the server’s privileged role is mainly faucet drip and operational relays.

**Overall assessment:** The project is **technically serious for its stage**. Economic rules are largely encoded in protocol constants and amendment-gated transactors, with drop-conservation style invariants and integer arithmetic on critical paths. Public documentation states a **clear, auditable launch model**: bulk supply is protocol-controlled; a **2% genesis bootstrap** is public and founder-custodied. The network is **not** yet a finished, decentralised mainnet: bootstrap UNL, bridge/issuer operations, freeze soak, and ceremony execution remain open.

| Area | Grade (1–10) | One-line view |
|------|:------------:|---------------|
| Vision & differentiation | 8.5 | PQ-first XRPL + paid validators + keyless bulk treasury |
| Tokenomics design | 8.0 | Fixed supply; honest 2% bootstrap; CID emission |
| Protocol engineering | 8.0 | Mature XRPL base + careful economic delta |
| Cryptography (tx / identity) | 8.5 | Falcon-512 real and first-class |
| Economic security | 6.5 | Bond + double-sign slash; UNL still bootstrap |
| Bridge / stables | 6.0 | Multi-sig lock model proven on Sepolia; mainnet open |
| Portal security model | 7.0 | Non-custodial core; residual web-app risks |
| Documentation | 8.5 | Whitepaper, freeze/rehearsal, readiness materials |
| Decentralisation today | 5.0 | Expected for early testnet |
| Mainnet readiness | 5.5 | Freeze pins exist; public T0 not claimed |
| **Composite** | **~7.4** | Credible pre-mainnet stack |

**Publication takeaway:** Suitable for **community testing and technical evaluation**. Not a substitute for an independent formal audit before large real-value mainnet capital.

---

## 2. System overview

### 2.1 Protocol (`FalconLedger`)

| Layer | Role |
|-------|------|
| Consensus | XRPL-style **RPCA** retained |
| Cryptography | **Falcon** as standard for wallets, validations, node identity |
| Supply | **200B** hard cap; **196B** keyless treasury; **4B** genesis circulating |
| Incentives | Epoch emission (CID), PoPL split (validators + LPs), fee burn/share |
| Security economy | Min bond, unbond delay, composite scoring, double-sign slash |
| Markets | Built-in DEX/AMM + vault/lending transactors |
| Names | Optional bonded account names (`NameSet` / unbond / release) |

### 2.2 Portal (`Falcon-faucet-wallet`)

| Feature | Role |
|---------|------|
| Faucet | Rate-limited native drip (testnet) |
| Wallet | Passkey-gated Falcon keys; send/receive FALCON and F-USDC |
| Swap / pool | AMM instant swap, limit orders, LP deposit/withdraw |
| Lend | Vault supply/borrow/repay/claim; permissionless path on testnet |
| Bridge | Sepolia USDC ↔ F-USDC (IOU) via lock contract + relay |
| Rewards | Bond / claim UX for validators |
| Other | Explorer, board, arcade/game faucet, whitepaper |

---

## 3. Tokenomics review

### 3.1 Hard supply split

| Bucket | Amount | Share | Control |
|--------|-------:|------:|---------|
| Protocol treasury | 196,000,000,000 | 98% | **No private key** — emission via protocol rules only |
| Genesis circulating | 4,000,000,000 | 2% | Split into three bootstrap wallets (below) |
| **Total** | **200,000,000,000** | 100% | Compile-time fixed |

The 98% treasury design is the project’s main structural answer to “company escrow of tens of billions.” Funds leave only through **RewardEpoch** / claim paths under the Proof-of-Participation feature set—not through discretionary foundation withdrawals.

### 3.2 Genesis 2% — public bootstrap

| Wallet | Amount | % of total | Purpose |
|--------|-------:|-----------:|---------|
| **Airdrop** | 2,000,000,000 | 1.0% | Community / mainnet contributor distribution |
| **Faucet** | 1,000,000,000 | 0.5% | Free claims for onboarding and fees |
| **Builder** | 1,000,000,000 | 0.5% | Pay for core engineering; contributors (code, audits, outreach) |

**Fairness (third-party view):** This is a **small, transparent** launch float relative to total supply. Half of the 2% is community-facing (airdrop); a quarter is free access (faucet); a quarter compensates builders and helpers. Paying the people who ship the chain from a fixed 0.5% pot is standard industry practice when disclosed. It is **not** equivalent to a hidden multi-ten-percent team unlock schedule on the bulk of supply.

### 3.3 Custody of bootstrap wallets

Documentation states that airdrop, faucet, and builder wallets are **founder-controlled** (single-operator, cold/offline preferred). The stated rationale is **skin in the game** and avoiding multi-signature arrangements with untrusted co-signers who could collude against the launch float.

**Third-party view:**

- **Technically**, any key-held balance can be moved by the key holder. That is inherent to wallets.
- **Economically**, confining that power to a **capped, published 2%** is a different risk class than company control of ~40% of supply via escrow.
- **Operationally**, founder custody is a legitimate choice for a solo/small-team builder; the public obligation is **transparency** (publish addresses at ceremony) and **hygiene** (cold storage for airdrop/builder; warm keys only for faucet ops).
- **Separately**, multi-sig remains good practice for **third-party collateral** on an EVM bridge lock (see §5.3)—that is other people’s USDC, not the builder pot.

### 3.4 Emission and fees

| Parameter | Value |
|-----------|--------|
| Epoch length | 172,800 ledgers (~7 days at ~3.5s) |
| First claimable emission | Epoch **8** (epochs 1–7 quiet) |
| Model | **CID** — continuous per-epoch decline on remaining treasury |
| Year-1 target | ~12% of remaining treasury / year (average) |
| Long-term floor | ~1.5% / year |
| PoPL split | Vault LPs and AMM LPs grow with participation (each capped); validators take remainder |
| Validator pay | **All bonded** with composite score, **pro-rata to score** (no top-K pay cut) |
| Min claim score | 5% composite (500 bps) |
| Fee burn | 40–70% dynamic; remainder toward validators |
| Min validator bond | 1,000 qXRP |
| Unbond lock | ~30 days (262,800 ledgers) |

Arithmetic on these paths is designed as **integer / bps** math (no floating point in core economic formulas). Claim paths use **pull-based** claims with **pool hard-caps** so order of claims does not invent extra supply beyond the epoch commitment.

### 3.5 Tokenomics risks (honest)

| Risk | Severity | Comment |
|------|----------|---------|
| Year-1 emission rate is high in absolute terms if participation is wide | Medium | Market absorption / secondary liquidity matters at mainnet |
| Free faucet can be farmed | Medium | Rate limits reduce abuse; do not eliminate Sybil |
| Founder holds 2% keys | Accepted if disclosed | Mitigate with cold storage + public addresses |
| Bootstrap UNL ≠ open validator set | Medium | Pay can be open to all bonded; **consensus trust** is still list-based until open-UNL |
| Governance surface is narrow | Low–Medium | Burn BPS today; most parameters are code/freeze |

---

## 4. Protocol functions and design quality

### 4.1 Custom economic surface (high level)

- Validator lifecycle: register, bond, unbond, release bond  
- Rewards: `ClaimReward`, LP / AMM LP claim paths under PoPL  
- Slash: **double-sign** enforced with dual STValidation evidence; full bond burn (no slasher bounty)  
- Governance: proposals/votes weighted by composite score; supermajority for bounded params  
- Account names: bond, cooldown, release  

### 4.2 Positive engineering signals

- New economics **amendment-gated** rather than always-on silent forks  
- Separate PQ key types and wire prefixes; secret zeroization patterns  
- liboqs used for Falcon rather than a bespoke crypto stack  
- Drop conservation / hard-cap thinking on claim and treasury paths  
- Rehearsal / freeze / ceremony documentation culture (image digests, dry-run artifacts)  
- Dress-rehearsal and smoke exercises for scoring, claims, names, bridge multi-sig  

### 4.3 Limitations

- Only **double-sign** slashing is live; other offense codes are staged (`temDISABLED` until detection is robust)  
- Open/rotating UNL is a **future** amendment; early networks use a bootstrap trust list  
- Lending and bridge depend on correct operator configuration and (for stables) off-ledger components  

---

## 5. Security review

### 5.1 Protocol

**Strengths**

- Falcon as first-class signing for user transactions and validator identity  
- Consensus proposal verification in current tree routes Falcon keys through the PQ verify path (operators should still pin and re-verify on the exact mainnet image)  
- ClaimReward checks bond ownership, bond status, minimum score, epoch non-duplication, treasury balance, and remaining pool  
- Slash evidence requires matching key, same ledger sequence, different ledger hash, valid signatures  
- Slashed funds are burned (anti-grief vs paid slasher)  
- Security contact and scope documented (`SECURITY.md`)  
- Prior self/third-party style audit artifacts exist in-repo for testnet eras  

**Risks**

| ID | Severity | Finding |
|----|----------|---------|
| S-01 | Medium | **Bootstrap UNL** concentrates consensus trust until open-UNL ships |
| S-02 | Medium | **F-USDC mint/issuer + bridge relay** are operational trust anchors for stables |
| S-03 | Medium | Pre-mainnet **assurance gap**: long sanitizer campaigns, high coverage on all qXRP delta, formal external freeze audit not assumed complete |
| S-04 | Low | Incomplete slash offence set by design |
| S-05 | Info | GitHub code scanning not observed as enabled |
| S-06 | Accepted | **Founder custody of 2%** — publish and cold-store; do not pretend keyless |

### 5.2 Smart contract (EVM lock)

`FalconCollateralLock.sol` implements **N-of-M multi-sig** for privileged release/withdraw/owner changes, with 1-of-1 aliases only when `required == 1` (testnet convenience). Sepolia multi-sig path has been exercised. **Ethereum mainnet lock is not the production state** until redeployed with cold owners and real USDC.

**Note:** Multi-sig here protects **locked USDC**. It is not a requirement that the founder multi-sig the **builder pot**.

### 5.3 Portal

**Strengths**

- Primary wallet path is **non-custodial**: Falcon keygen/sign in browser WASM  
- Dangerous endpoints fail closed (server wallet propose / legacy sign / server derive disabled by default in production)  
- Origin checks, faucet quotas, arcade claims based on **server-stored scores** (body score ignored)  
- Production preference for HTTPS on RPC/submit paths  

**Risks**

| ID | Severity | Finding |
|----|----------|---------|
| W-01 | Medium | WebAuthn **PRF fallback** weakens encryption when PRF unavailable |
| W-02 | Medium | CSP / XSS residual risk if `unsafe-inline` remains |
| W-03 | Medium | Faucet and game faucet **Sybil economics** |
| W-04 | Medium | Compromise of **faucet secret / signer proxy token** drains faucet budget |
| W-05 | Low | Backup KDF / plaintext restore edge cases (testnet hygiene → mainnet tighten) |

---

## 6. Trust model (how to read Falcon Ledger)

Keep these layers separate when evaluating “who can do what”:

```
┌─────────────────────────────────────────────────────────────┐
│  98% PROTOCOL TREASURY                                      │
│  No private key · epoch rules only · CID + PoPL claims      │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│  2% BOOTSTRAP (4B)                                          │
│  Airdrop 2B · Faucet 1B · Builder 1B · founder keys         │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│  USER WALLETS (portal)                                      │
│  Self-custody Falcon · server never needs falcon_secret     │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│  STABLES / BRIDGE                                           │
│  Issuer + relay + EVM lock multi-sig for USDC               │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│  CONSENSUS UNL                                              │
│  Bootstrap list today · open/rotating UNL later             │
└─────────────────────────────────────────────────────────────┘
```

A fair summary of the project’s public claims:

- **Accurate:** No company escrow of bulk supply; 98% keyless; emissions by rules; validators paid; Falcon-first; 2% bootstrap is public and small.  
- **Not claimed (and should not be over-read):** Zero human involvement at launch; trustless bridge; fully open UNL on day one.

---

## 7. Product completeness (portal)

For a pre-mainnet network, the portal surface is **unusually complete**: faucet → funded wallet → swap/pool → lend → bridge → rewards. That reduces dependence on centralised exchanges for day-one UX, which matches the whitepaper goal.

**Trade-off:** Wide surface area increases web-app and ops attack surface (arcade, board, many APIs). Prioritise hardening of **money paths** (sign, submit, faucet, bridge, lend) over growth features when approaching mainnet.

---

## 8. Mainnet readiness

The project’s own public readiness materials describe protocol freeze pins and rehearsal work; they do **not** claim public mainnet is live.

**Typical remaining gates before real-value T0:**

1. Multi-day soak on the freeze image  
2. Ceremony: genesis split to airdrop / faucet / builder; **publish addresses**  
3. Deploy ETH mainnet lock with cold multi-sig for USDC  
4. Fund faucet from the faucet wallet; keep portal mainnet flag off until consensus is green  
5. Independent freeze-scope review (recommended for external capital)  
6. Document open-UNL timeline  

---

## 9. Recommendations

### For the project

1. **Ceremony transparency** — publish AIRDROP, FAUCET, and BUILDER addresses and balances after split.  
2. **Key hygiene** — cold storage for airdrop and builder; minimal hot balance for faucet.  
3. **Bridge** — multi-sig cold owners for **USDC lock**; never reuse testnet host keys.  
4. **External freeze audit** before large mainnet capital.  
5. **Open-UNL roadmap** with explicit milestones.  
6. **Portal** — tighten CSP; fail closed when WebAuthn PRF is missing; continue rate-limit discipline.  
7. **Repo hygiene** — enable secret scanning / Dependabot; keep freeze digests and network IDs consistent across docs.  

### For users and community

1. Treat **testnet** as play money and learning.  
2. Prefer **self-custody** Falcon wallets; never paste `falcon_secret` into unknown sites.  
3. Understand **F-USDC** is an IOU + bridge system, not the keyless 98% treasury.  
4. Read the **2% bootstrap** as intentional founder accountability, not as “unlimited team mint.”  

---

## 10. Comparative note (vs XRP Ledger)

| Dimension | XRP Ledger (typical critique) | Falcon Ledger (as designed) |
|-----------|-------------------------------|-----------------------------|
| Bulk supply | Large company/escrow overhang historically | **98% keyless protocol treasury** |
| Launch float | Large founder/company allocations historically | **2% public: airdrop / faucet / builder** |
| Validator pay | Generally none | **Epoch rewards ∝ on-ledger score** |
| Signatures | Classical curves by default | **Falcon-512 standard** |
| Selling rewards | Often needs CEX | **In-wallet DEX/AMM path targeted** |
| Early decentralisation | Strong ecosystem age | **Early network: bootstrap UNL** |

---

## 11. Closing statement

Falcon Ledger is a **credible, engineering-led attempt** to ship a fast XRPL-class ledger with **post-quantum signatures**, **validator-aligned emissions**, and a **structurally constrained supply model**: almost all tokens cannot be moved by a company key; a **small, disclosed 2%** funds community access and pays builders.

The companion portal is **production-shaped for testnet**: non-custodial Falcon wallet plus markets and bridge enough to use the network without a centralised exchange.

**For publication:** this is a **strong pre-mainnet project** with clear public economics and real code depth—not a paper-only fork. Residual risks are those of any honest early network: **bootstrap consensus trust**, **stables bridge ops**, **web-app security**, and **execution of mainnet ceremony**. Those do not invalidate the design; they define the work left before large real-value use.

---

### Disclaimer

This review is an independent technical assessment based on publicly accessible repository content and documentation as of the review date. It is **not** investment advice, **not** a guarantee of security, and **not** a formal audit report with liability. Features, network state, and operational practices may change.

---

*Falcon Ledger · qXRP · Review date 2026-07-23*
