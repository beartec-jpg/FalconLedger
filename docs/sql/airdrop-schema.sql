-- Falcon Ledger mainnet — portal airdrop + faucet durable schema
-- Apply with: psql "$DATABASE_URL" -f docs/sql/airdrop-schema.sql
-- Safe to re-run (IF NOT EXISTS).

BEGIN;

-- Faucet claim log (enforce 5/day + 1h cooldown in app; this is source of truth)
CREATE TABLE IF NOT EXISTS faucet_claims (
    id              BIGSERIAL PRIMARY KEY,
    network         TEXT NOT NULL DEFAULT 'mainnet',
    account         TEXT NOT NULL,
    ip_hash         TEXT,
    amount_qxrp     NUMERIC NOT NULL,
    tx_hash         TEXT,
    claimed_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS faucet_claims_account_time_idx
    ON faucet_claims (network, account, claimed_at DESC);

CREATE INDEX IF NOT EXISTS faucet_claims_ip_time_idx
    ON faucet_claims (network, ip_hash, claimed_at DESC);

-- Airdrop global config (single row per network)
CREATE TABLE IF NOT EXISTS airdrop_config (
    network             TEXT PRIMARY KEY,
    genesis_at          TIMESTAMPTZ,
    window_days         INT NOT NULL DEFAULT 60,
    pool_falcon         NUMERIC NOT NULL DEFAULT 2000000000,
    first_emission_epoch INT NOT NULL DEFAULT 8,
    frozen_at           TIMESTAMPTZ,
    notes               TEXT,
    updated_at          TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

INSERT INTO airdrop_config (network, pool_falcon, first_emission_epoch)
VALUES ('mainnet', 2000000000, 8)
ON CONFLICT (network) DO NOTHING;

-- Daily snapshots
CREATE TABLE IF NOT EXISTS airdrop_snapshots (
    id              BIGSERIAL PRIMARY KEY,
    network         TEXT NOT NULL DEFAULT 'mainnet',
    snap_day        DATE NOT NULL,
    payload         JSONB NOT NULL DEFAULT '{}',
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (network, snap_day)
);

-- Per-address running scores / final allocations
CREATE TABLE IF NOT EXISTS airdrop_allocations (
    network         TEXT NOT NULL DEFAULT 'mainnet',
    account         TEXT NOT NULL,
    score           NUMERIC NOT NULL DEFAULT 0,
    falcon_amount   NUMERIC,
    paid_tx_hash    TEXT,
    paid_at         TIMESTAMPTZ,
    meta            JSONB NOT NULL DEFAULT '{}',
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (network, account)
);

CREATE INDEX IF NOT EXISTS airdrop_allocations_score_idx
    ON airdrop_allocations (network, score DESC);

COMMIT;
