// This file is auto-generated. Do not edit.
#pragma once

#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/STParsedJSON.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol_autogen/TransactionBase.h>
#include <xrpl/protocol_autogen/TransactionBuilderBase.h>
#include <xrpl/json/json_value.h>

#include <stdexcept>
#include <optional>

namespace xrpl::transactions {

class LoanCollateralDepositBuilder;

/**
 * @brief Transaction: LoanCollateralDeposit
 *
 * Type: ttLOAN_COLLATERAL_DEPOSIT (83)
 * Delegable: Delegation::NotDelegable
 * Amendment: featureLendingCollateral
 * Privileges: NoPriv
 *
 * Immutable wrapper around STTx providing type-safe field access.
 * Use LoanCollateralDepositBuilder to construct new transactions.
 */
class LoanCollateralDeposit : public TransactionBase
{
public:
    static constexpr xrpl::TxType txType = ttLOAN_COLLATERAL_DEPOSIT;

    /**
     * @brief Construct a LoanCollateralDeposit transaction wrapper from an existing STTx object.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    explicit LoanCollateralDeposit(std::shared_ptr<STTx const> tx)
        : TransactionBase(std::move(tx))
    {
        // Verify transaction type
        if (tx_->getTxnType() != txType)
        {
            throw std::runtime_error("Invalid transaction type for LoanCollateralDeposit");
        }
    }

    // Transaction-specific field getters

    /**
     * @brief Get sfLoanID (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT256::type::value_type
    getLoanID() const
    {
        return this->tx_->at(sfLoanID);
    }

    /**
     * @brief Get sfCollateral (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_AMOUNT::type::value_type
    getCollateral() const
    {
        return this->tx_->at(sfCollateral);
    }
};

/**
 * @brief Builder for LoanCollateralDeposit transactions.
 *
 * Provides a fluent interface for constructing transactions with method chaining.
 * Uses STObject internally for flexible transaction construction.
 * Inherits common field setters from TransactionBuilderBase.
 */
class LoanCollateralDepositBuilder : public TransactionBuilderBase<LoanCollateralDepositBuilder>
{
public:
    /**
     * @brief Construct a new LoanCollateralDepositBuilder with required fields.
     * @param account The account initiating the transaction.
     * @param loanID The sfLoanID field value.
     * @param collateral The sfCollateral field value.
     * @param sequence Optional sequence number for the transaction.
     * @param fee Optional fee for the transaction.
     */
    LoanCollateralDepositBuilder(SF_ACCOUNT::type::value_type account,
                     std::decay_t<typename SF_UINT256::type::value_type> const& loanID,                     std::decay_t<typename SF_AMOUNT::type::value_type> const& collateral,                    std::optional<SF_UINT32::type::value_type> sequence = std::nullopt,
                    std::optional<SF_AMOUNT::type::value_type> fee = std::nullopt
)
        : TransactionBuilderBase<LoanCollateralDepositBuilder>(ttLOAN_COLLATERAL_DEPOSIT, account, sequence, fee)
    {
        setLoanID(loanID);
        setCollateral(collateral);
    }

    /**
     * @brief Construct a LoanCollateralDepositBuilder from an existing STTx object.
     * @param tx The existing transaction to copy from.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    LoanCollateralDepositBuilder(std::shared_ptr<STTx const> tx)
    {
        if (tx->getTxnType() != ttLOAN_COLLATERAL_DEPOSIT)
        {
            throw std::runtime_error("Invalid transaction type for LoanCollateralDepositBuilder");
        }
        object_ = *tx;
    }

    /** @brief Transaction-specific field setters */

    /**
     * @brief Set sfLoanID (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    LoanCollateralDepositBuilder&
    setLoanID(std::decay_t<typename SF_UINT256::type::value_type> const& value)
    {
        object_[sfLoanID] = value;
        return *this;
    }

    /**
     * @brief Set sfCollateral (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    LoanCollateralDepositBuilder&
    setCollateral(std::decay_t<typename SF_AMOUNT::type::value_type> const& value)
    {
        object_[sfCollateral] = value;
        return *this;
    }

    /**
     * @brief Build and return the LoanCollateralDeposit wrapper.
     * @param publicKey The public key for signing.
     * @param secretKey The secret key for signing.
     * @return The constructed transaction wrapper.
     */
    LoanCollateralDeposit
    build(PublicKey const& publicKey, SecretKey const& secretKey)
    {
        sign(publicKey, secretKey);
        return LoanCollateralDeposit{std::make_shared<STTx>(std::move(object_))};
    }
};

}  // namespace xrpl::transactions
