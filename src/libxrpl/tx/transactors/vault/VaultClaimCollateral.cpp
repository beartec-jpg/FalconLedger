#include <xrpl/tx/transactors/vault/VaultClaimCollateral.h>

#include <xrpl/basics/Log.h>
#include <xrpl/beast/utility/Zero.h>
#include <xrpl/ledger/View.h>
#include <xrpl/ledger/helpers/LendingHelpers.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFlags.h>
#include <xrpl/tx/Transactor.h>

#include <memory>

namespace xrpl {

bool
VaultClaimCollateral::checkExtraFeatures(PreflightContext const& ctx)
{
    return ctx.rules.enabled(featureLendingPermissionless) &&
        ctx.rules.enabled(featureLendingCollateral) &&
        checkLendingProtocolDependencies(ctx.rules, ctx.tx);
}

std::uint32_t
VaultClaimCollateral::getFlagsMask(PreflightContext const& ctx)
{
    return tfUniversal;
}

NotTEC
VaultClaimCollateral::preflight(PreflightContext const& ctx)
{
    if (ctx.tx[sfLoanBrokerID] == beast::kZERO)
        return temINVALID;

    if (ctx.tx.isFieldPresent(sfAmount))
    {
        auto const amount = ctx.tx[sfAmount];
        if (!amount.native() || amount <= beast::kZERO)
            return temBAD_AMOUNT;
    }

    return tesSUCCESS;
}

TER
VaultClaimCollateral::preclaim(PreclaimContext const& ctx)
{
    auto const& tx = ctx.tx;
    auto const account = tx[sfAccount];
    auto const brokerID = tx[sfLoanBrokerID];

    auto const brokerSle = ctx.view.read(keylet::loanbroker(brokerID));
    if (!brokerSle)
    {
        JLOG(ctx.j.warn()) << "LoanBroker does not exist.";
        return tecNO_ENTRY;
    }

    auto const vaultSle = ctx.view.read(keylet::vault(brokerSle->at(sfVaultID)));
    if (!vaultSle)
        return tefBAD_LEDGER;

    auto const pending =
        Lending::liquidationPendingFalcon(ctx.view, vaultSle, account);
    if (pending <= beast::kZERO)
    {
        JLOG(ctx.j.warn()) << "No pending liquidation FALCON for account.";
        return tecUNFUNDED_PAYMENT;
    }

    return tesSUCCESS;
}

TER
VaultClaimCollateral::doApply()
{
    auto const& tx = ctx_.tx;
    auto& view = ctx_.view();

    auto const account = tx[sfAccount];
    auto const brokerID = tx[sfLoanBrokerID];

    auto const brokerSle = view.read(keylet::loanbroker(brokerID));
    if (!brokerSle)
        return tefBAD_LEDGER;

    auto vaultSle = view.peek(keylet::vault(brokerSle->at(sfVaultID)));
    if (!vaultSle)
        return tefBAD_LEDGER;

    std::optional<STAmount> maxAmount;
    if (tx.isFieldPresent(sfAmount))
        maxAmount = tx[sfAmount];

    return Lending::claimLiquidationFalcon(
        view, vaultSle, brokerSle->at(sfAccount), account, maxAmount, j_);
}

void
VaultClaimCollateral::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
VaultClaimCollateral::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

}  // namespace xrpl
