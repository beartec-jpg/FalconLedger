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

class ValidatorUnbondBuilder;

/**
 * @brief Transaction: ValidatorUnbond
 *
 * Type: ttVALIDATOR_UNBOND (87)
 * Delegable: Delegation::NotDelegable
 * Amendment: featureProofOfParticipation
 * Privileges: NoPriv
 *
 * Immutable wrapper around STTx providing type-safe field access.
 * Use ValidatorUnbondBuilder to construct new transactions.
 */
class ValidatorUnbond : public TransactionBase
{
public:
    static constexpr xrpl::TxType txType = ttVALIDATOR_UNBOND;

    /**
     * @brief Construct a ValidatorUnbond transaction wrapper from an existing STTx object.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    explicit ValidatorUnbond(std::shared_ptr<STTx const> tx)
        : TransactionBase(std::move(tx))
    {
        // Verify transaction type
        if (tx_->getTxnType() != txType)
        {
            throw std::runtime_error("Invalid transaction type for ValidatorUnbond");
        }
    }

    // Transaction-specific field getters

    /**
     * @brief Get sfConsensusKey (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_VL::type::value_type
    getConsensusKey() const
    {
        return this->tx_->at(sfConsensusKey);
    }
};

/**
 * @brief Builder for ValidatorUnbond transactions.
 *
 * Provides a fluent interface for constructing transactions with method chaining.
 * Uses STObject internally for flexible transaction construction.
 * Inherits common field setters from TransactionBuilderBase.
 */
class ValidatorUnbondBuilder : public TransactionBuilderBase<ValidatorUnbondBuilder>
{
public:
    /**
     * @brief Construct a new ValidatorUnbondBuilder with required fields.
     * @param account The account initiating the transaction.
     * @param consensusKey The sfConsensusKey field value.
     * @param sequence Optional sequence number for the transaction.
     * @param fee Optional fee for the transaction.
     */
    ValidatorUnbondBuilder(SF_ACCOUNT::type::value_type account,
                     std::decay_t<typename SF_VL::type::value_type> const& consensusKey,                    std::optional<SF_UINT32::type::value_type> sequence = std::nullopt,
                    std::optional<SF_AMOUNT::type::value_type> fee = std::nullopt
)
        : TransactionBuilderBase<ValidatorUnbondBuilder>(ttVALIDATOR_UNBOND, account, sequence, fee)
    {
        setConsensusKey(consensusKey);
    }

    /**
     * @brief Construct a ValidatorUnbondBuilder from an existing STTx object.
     * @param tx The existing transaction to copy from.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    ValidatorUnbondBuilder(std::shared_ptr<STTx const> tx)
    {
        if (tx->getTxnType() != ttVALIDATOR_UNBOND)
        {
            throw std::runtime_error("Invalid transaction type for ValidatorUnbondBuilder");
        }
        object_ = *tx;
    }

    /** @brief Transaction-specific field setters */

    /**
     * @brief Set sfConsensusKey (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorUnbondBuilder&
    setConsensusKey(std::decay_t<typename SF_VL::type::value_type> const& value)
    {
        object_[sfConsensusKey] = value;
        return *this;
    }

    /**
     * @brief Build and return the ValidatorUnbond wrapper.
     * @param publicKey The public key for signing.
     * @param secretKey The secret key for signing.
     * @return The constructed transaction wrapper.
     */
    ValidatorUnbond
    build(PublicKey const& publicKey, SecretKey const& secretKey)
    {
        sign(publicKey, secretKey);
        return ValidatorUnbond{std::make_shared<STTx>(std::move(object_))};
    }
};

}  // namespace xrpl::transactions
