"""Shared guards for coordinator bootstrap scripts (testnet vs mainnet launch)."""

from __future__ import annotations

import os

# Falcon Ledger testnet (custom network). All other IDs are treated as production launch.
TESTNET_NETWORK_IDS = frozenset({1001})


def bridge_only_required(network_id: int) -> bool:
    """True when issuer bootstrap mint must not run (mainnet / production launch)."""
    if network_id not in TESTNET_NETWORK_IDS:
        return True
    return os.environ.get("FALCON_BRIDGE_ONLY_REQUIRED", "").strip().lower() in (
        "1",
        "true",
        "yes",
    )


def is_testnet_network(network_id: int) -> bool:
    return network_id in TESTNET_NETWORK_IDS