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

class ReleaseBondBuilder;

/**
 * @brief Transaction: ReleaseBond
 *
 * Type: ttRELEASE_BOND (90)
 * Delegable: Delegation::NotDelegable
 * Amendment: featureProofOfParticipation
 * Privileges: NoPriv
 *
 * Immutable wrapper around STTx providing type-safe field access.
 * Use ReleaseBondBuilder to construct new transactions.
 */
class ReleaseBond : public TransactionBase
{
public:
    static constexpr xrpl::TxType txType = ttRELEASE_BOND;

    /**
     * @brief Construct a ReleaseBond transaction wrapper from an existing STTx object.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    explicit ReleaseBond(std::shared_ptr<STTx const> tx)
        : TransactionBase(std::move(tx))
    {
        // Verify transaction type
        if (tx_->getTxnType() != txType)
        {
            throw std::runtime_error("Invalid transaction type for ReleaseBond");
        }
    }

    // Transaction-specific field getters

    /**
     * @brief Get sfSlashTarget (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_ACCOUNT::type::value_type
    getSlashTarget() const
    {
        return this->tx_->at(sfSlashTarget);
    }
};

/**
 * @brief Builder for ReleaseBond transactions.
 *
 * Provides a fluent interface for constructing transactions with method chaining.
 * Uses STObject internally for flexible transaction construction.
 * Inherits common field setters from TransactionBuilderBase.
 */
class ReleaseBondBuilder : public TransactionBuilderBase<ReleaseBondBuilder>
{
public:
    /**
     * @brief Construct a new ReleaseBondBuilder with required fields.
     * @param account The account initiating the transaction.
     * @param slashTarget The sfSlashTarget field value.
     * @param sequence Optional sequence number for the transaction.
     * @param fee Optional fee for the transaction.
     */
    ReleaseBondBuilder(SF_ACCOUNT::type::value_type account,
                     std::decay_t<typename SF_ACCOUNT::type::value_type> const& slashTarget,                    std::optional<SF_UINT32::type::value_type> sequence = std::nullopt,
                    std::optional<SF_AMOUNT::type::value_type> fee = std::nullopt
)
        : TransactionBuilderBase<ReleaseBondBuilder>(ttRELEASE_BOND, account, sequence, fee)
    {
        setSlashTarget(slashTarget);
    }

    /**
     * @brief Construct a ReleaseBondBuilder from an existing STTx object.
     * @param tx The existing transaction to copy from.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    ReleaseBondBuilder(std::shared_ptr<STTx const> tx)
    {
        if (tx->getTxnType() != ttRELEASE_BOND)
        {
            throw std::runtime_error("Invalid transaction type for ReleaseBondBuilder");
        }
        object_ = *tx;
    }

    /** @brief Transaction-specific field setters */

    /**
     * @brief Set sfSlashTarget (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ReleaseBondBuilder&
    setSlashTarget(std::decay_t<typename SF_ACCOUNT::type::value_type> const& value)
    {
        object_[sfSlashTarget] = value;
        return *this;
    }

    /**
     * @brief Build and return the ReleaseBond wrapper.
     * @param publicKey The public key for signing.
     * @param secretKey The secret key for signing.
     * @return The constructed transaction wrapper.
     */
    ReleaseBond
    build(PublicKey const& publicKey, SecretKey const& secretKey)
    {
        sign(publicKey, secretKey);
        return ReleaseBond{std::make_shared<STTx>(std::move(object_))};
    }
};

}  // namespace xrpl::transactions
