#include <xrpl/tx/transactors/lending/LoanCollateralDeposit.h>

#include <xrpl/basics/Log.h>
#include <xrpl/beast/utility/Zero.h>
#include <xrpl/ledger/View.h>
#include <xrpl/ledger/helpers/LendingHelpers.h>
#include <xrpl/ledger/helpers/TokenHelpers.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFlags.h>
#include <xrpl/protocol/XRPAmount.h>
#include <xrpl/tx/Transactor.h>

#include <memory>

namespace xrpl {

bool
LoanCollateralDeposit::checkExtraFeatures(PreflightContext const& ctx)
{
    return ctx.rules.enabled(featureLendingCollateral) &&
        checkLendingProtocolDependencies(ctx.rules, ctx.tx);
}

std::uint32_t
LoanCollateralDeposit::getFlagsMask(PreflightContext const& ctx)
{
    return tfUniversal;
}

NotTEC
LoanCollateralDeposit::preflight(PreflightContext const& ctx)
{
    if (ctx.tx[sfLoanID] == beast::kZERO)
        return temINVALID;

    if (!ctx.tx.isFieldPresent(sfCollateral))
        return temINVALID;

    auto const collateral = ctx.tx[sfCollateral];
    if (!collateral.native() || collateral <= beast::kZERO)
        return temBAD_AMOUNT;

    return tesSUCCESS;
}

TER
LoanCollateralDeposit::preclaim(PreclaimContext const& ctx)
{
    auto const& tx = ctx.tx;
    auto const account = tx[sfAccount];
    auto const loanID = tx[sfLoanID];
    auto const collateral = tx[sfCollateral];

    auto const loanSle = ctx.view.read(keylet::loan(loanID));
    if (!loanSle)
    {
        JLOG(ctx.j.warn()) << "Loan does not exist.";
        return tecNO_ENTRY;
    }

    if (loanSle->at(sfBorrower) != account)
    {
        JLOG(ctx.j.warn()) << "Only the borrower may add collateral to a loan.";
        return tecNO_PERMISSION;
    }

    if (loanSle->at(sfPaymentRemaining) == 0 || loanSle->at(sfPrincipalOutstanding) == 0)
    {
        JLOG(ctx.j.warn()) << "Loan is already paid off.";
        return tecKILLED;
    }

    auto const loanBrokerID = loanSle->at(sfLoanBrokerID);
    auto const loanBrokerSle = ctx.view.read(keylet::loanbroker(loanBrokerID));
    if (!loanBrokerSle)
        return tefBAD_LEDGER;

    auto const borrowerSle = ctx.view.read(keylet::account(account));
    if (!borrowerSle)
        return terNO_ACCOUNT;

    auto const balance = borrowerSle->at(sfBalance).value().xrp();
    if (balance < collateral.xrp())
    {
        JLOG(ctx.j.warn()) << "Insufficient FALCON for collateral deposit.";
        return tecINSUFFICIENT_FUNDS;
    }

    return tesSUCCESS;
}

TER
LoanCollateralDeposit::doApply()
{
    auto const& tx = ctx_.tx;
    auto& view = ctx_.view();

    auto const loanID = tx[sfLoanID];
    auto const collateral = tx[sfCollateral];

    auto loanSle = view.peek(keylet::loan(loanID));
    if (!loanSle)
        return tefBAD_LEDGER;

    auto const brokerID = loanSle->at(sfLoanBrokerID);
    auto const brokerSle = view.read(keylet::loanbroker(brokerID));
    if (!brokerSle)
        return tefBAD_LEDGER;

    auto const brokerPseudo = brokerSle->at(sfAccount);
    auto const borrower = loanSle->at(sfBorrower);

    if (auto const ter = transferXRP(view, borrower, brokerPseudo, collateral, j_))
        return ter;

    STAmount locked = beast::kZERO;
    if (loanSle->isFieldPresent(sfCollateral))
        locked = loanSle->at(sfCollateral);
    loanSle->at(sfCollateral) = locked + collateral;
    view.update(loanSle);

    return tesSUCCESS;
}

void
LoanCollateralDeposit::visitInvariantEntry(
    bool,
    std::shared_ptr<SLE const> const&,
    std::shared_ptr<SLE const> const&)
{
}

bool
LoanCollateralDeposit::finalizeInvariants(
    STTx const&,
    TER,
    XRPAmount,
    ReadView const&,
    beast::Journal const&)
{
    return true;
}

//------------------------------------------------------------------------------

}  // namespace xrpl