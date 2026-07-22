# Account name service (human names → r-address)

Status: **implemented in protocol** (2026-07-22) · amendment `AccountNames` · txs `NameSet`/`NameUnbond`/`NameRelease`  
Network role: optional UX layer on top of Falcon wallets (`r…` remains the settlement address).  
Enable: add `AccountNames` under `[features]` (mainnet cfg example includes it).

---

## Product rules (locked)

| Rule | Value |
|------|--------|
| Bond amount | **100 FALCON** (100_000_000 drops) |
| Who can register | Any **funded** account (can pay fee + bond) |
| Names per account | **One** active name per account |
| Unbond | Releases the name (**free on unbond**) |
| Cooldown | **1 epoch** after unbond starts before bond returns and name is free for others |
| Payments during release | **Reject** resolution / name-routed payments while status is releasing |

Epoch length follows the network pin:

- Mainnet / long-epoch: 172_800 ledgers (~7 days)  
- Rehearsal fast-epoch: 256 ledgers (test only)

---

## User flow

1. **Create wallet** — Falcon keypair → `r…` (no name required).  
2. **Fund** — receive FALCON.  
3. **Claim name** — choose e.g. `scott.reynolds` → if free and account has no name → lock **100 FALCON** bond → name live.  
4. **Send** — pay to `alice.bob` → wallet resolves to her `r…` → normal Payment.  
5. **Release** — owner starts unbond → status **releasing** for **1 epoch** → then bond returned, name free, another account may claim it.

---

## Name format (recommended)

- Lowercase ASCII `a-z`, digits `0-9`, single `.` separator optional  
- Example: `scott.reynolds`, `alice.bob`  
- Length bounds: e.g. 3–32 chars total (tunable)  
- No leading/trailing `.`, no consecutive `..`  
- Case-insensitive: store and compare normalized lowercase  

---

## On-ledger objects

### `ltACCOUNT_NAME` (singleton per name)

| Field | Meaning |
|-------|---------|
| `sfName` | Normalized name string (or hash keylet + name field) |
| `sfAccount` | Owner `r…` |
| `sfBondedAmount` | 100 FALCON while active / releasing |
| `sfNameStatus` | `0 = active`, `1 = releasing` |
| `sfUnbondingStartLedger` | Ledger when release started (0 if active) |
| `sfPreviousTxnID` / `sfPreviousTxnLgrSeq` | Standard |

**Keylet:** `keylet::accountName(normalizedName)` (hash of name bytes).

### Owner side

- At most one active/releasing name per account: enforce via owner directory entry or `sfAccountName` on account root (prefer explicit name object + owner dir for reserve accounting).

---

## Transactions

### `NameSet` (claim / register)

- **Account** must not already own a name (active or releasing).  
- **Name** free (no object, or fully released).  
- Debit **100 FALCON** from Account → `sfBondedAmount` on name object.  
- Status = **active**.  
- Failures: `tecDUPLICATE` (name taken / already own one), `tecUNFUNDED`, `temMALFORMED` (bad name).

### `NameUnbond` (start release)

- Only owner.  
- Status must be **active**.  
- Status → **releasing**, set `sfUnbondingStartLedger = view.seq()`.  
- Name stays reserved to owner until cooldown ends (no one else can `NameSet` it yet).  
- **Name-routed payments reject** while releasing.

### `NameRelease` (pull model — implemented)

Owner (or anyone with the name) submits `NameRelease` when  
`view.seq() >= UnbondingStartLedger + kNAME_UNBOND_LEDGERS` (one epoch).  
Then: return bond to owner, delete name object (name free). Early release → `tecTOO_SOON`.

---

## Payment / resolution rules

| Situation | Behavior |
|-----------|----------|
| Resolve `alice.bob` → active | Return owner `r…` |
| Resolve while **releasing** | **Reject** (UI error: “name releasing”; do not send) |
| Resolve unknown name | Reject / not found |
| Payment to raw `r…` | Always allowed (unchanged) |
| Payment with Destination = name string | Only if wallet expands name first; chain Payment still uses AccountID |

Optional later: destination tag / memo of name for human audit — not required for v1.

---

## Cooldown timeline (1 epoch)

```
t0  NameUnbond     status=releasing, payments by name REJECTED
    …
t0 + 1 epoch       NameRelease allowed
                   bond → owner balance
                   name object deleted → free for others
```

“1 epoch” = one full `kQXRP_LEDGERS_PER_EPOCH` ledgers after `UnbondingStartLedger` (not calendar clock).

---

## Anti-squat / economics

- 100 FALCON opportunity cost while holding a name.  
- Unbond frees name after 1 epoch (no permanent squat without capital).  
- One name per account limits bulk hoarding per funded key (sybil still possible via many funded accounts — acceptable for v1).

---

## Portal UX (sketch)

1. Wallet create — no name step required.  
2. After balance ≥ 100 + reserve: **“Claim username”**.  
3. Send form: accept `r…` **or** `name` (if contains `.` or matches name regex → resolve).  
4. Profile: show name + status; **Release name** → confirm cooldown.  
5. During releasing: badge “Releasing — cannot receive by name”.

---

## Implementation status

| Piece | Status |
|-------|--------|
| Constants, keylet, `ltACCOUNT_NAME`, amendment `AccountNames` | **Done** (freeze pin) |
| `NameSet` / `NameUnbond` / `NameRelease` | **Done** — smoke PASS on mainnet-v1 |
| RPC `ledger_entry` by `account_name` | **Done** |
| Portal claim / release / send-by-name | **Pending** product UX |
| Full long-epoch NameRelease cooldown e2e | Deferred (long wait); `tecTOO_SOON` proven |

---

## Non-goals (v1)

- Vanity grinding of the `r…` string itself  
- Network-generated key material from username  
- Multi-name portfolios per account  
- Marketplace / name transfer (can add as `NameSet` with previous owner sign later)

---

## Sign-off

| Decision | Owner | Date |
|----------|--------|------|
| Bond 100 FALCON, 1/account, 1-epoch cooldown, free on unbond, reject pays while releasing | Product | 2026-07-22 |
