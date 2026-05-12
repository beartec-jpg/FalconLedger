# qXRP On-Chain Governance

## Overview

Governance allows the bonded validator set to update protocol parameters on-chain
without a hard fork.  The initial governable parameter is `sfCurrentBurnBps`
(the fee-split burn fraction).

## Amendment Gate

All governance transactions require the `ProofOfParticipation` amendment to be
active.  Governance objects are rejected with `temDISABLED` otherwise.

## Proposal Lifecycle

```
GovernanceProposal submitted
        │
        ▼
   Voting window opens
   (kGOVERNANCE_VOTING_LEDGERS = 172,800 ledgers ≈ 7 days)
        │
        ├─ YES votes accumulate (GovernanceVote)
        │
        ▼
   Voting window closes
        │
        ├─ YES weight ≥ kGOVERNANCE_SUPERMAJORITY_BPS (6 700 bps of aggregate score)?
        │        YES → parameter updated, proposal marked PASSED
        │        NO  → proposal marked EXPIRED, parameter unchanged
        ▼
   ltGOVERNANCE_PARAMS object updated (on PASSED)
```

## Proposal Types

| Type | `sfProposalType` | Target field |
|------|-----------------|-------------|
| Burn BPS change | 1 (`kPROPOSAL_TYPE_BURN_BPS`) | `sfCurrentBurnBps` |

Additional types will be added via future amendments.

## Voting Weight

Each vote is weighted by the validator's `sfCompositeScore` at the time of
submission.  Votes are counted as:

```
yesWeight  = Σ sfCompositeScore for YES votes
threshold  = sfAggregateCompositeScore * kGOVERNANCE_SUPERMAJORITY_BPS / kBPS_DENOM
passed     = yesWeight >= threshold
```

## Ledger Objects

### `ltGOVERNANCE_PROPOSAL`
Fields: `sfProposalID`, `sfProposalType`, `sfProposalValue`, `sfExpiry`,
        `sfYesWeight`, `sfProposalStatus`.

### `ltGOVERNANCE_VOTE`
Fields: `sfProposalID`, `sfAccount` (voter), `sfCompositeScore` (at vote time),
        `sfVoteValue` (YES=1).

### `ltGOVERNANCE_PARAMS`
Singleton.  Fields: `sfCurrentBurnBps`.  Created at genesis with
`sfCurrentBurnBps = kFEE_BURN_DEFAULT_BPS`.

## Security Properties

- **Replay protection:** Each `GovernanceVote` is keyed by `(ProposalID, AccountID)`;
  duplicate votes are rejected with `tecDUPLICATE`.
- **Score snapshot:** Score is read at vote submission time; late manipulation of
  composite score does not retroactively change vote weight.
- **Supermajority:** 67 % of aggregate score must approve — minority validators
  cannot unilaterally change parameters.
