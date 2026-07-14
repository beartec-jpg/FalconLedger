# Copyright (c) 2026 qXRP Team.
# SPDX-License-Identifier: AGPL-3.0-only
"""PoPL epoch loan duration constants (7 days × 52 per year)."""

EPOCH_DAYS = 7
EPOCH_SECONDS = EPOCH_DAYS * 86_400
EPOCHS_PER_YEAR = 52
DEFAULT_LOAN_EPOCHS = 1


def payment_interval_for_epochs(epochs: int) -> int:
    n = max(1, min(EPOCHS_PER_YEAR, int(epochs)))
    return n * EPOCH_SECONDS