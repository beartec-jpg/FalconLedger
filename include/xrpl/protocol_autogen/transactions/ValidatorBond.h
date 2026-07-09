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

class ValidatorBondBuilder;

/**
 * @brief Transaction: ValidatorBond
 *
 * Type: ttVALIDATOR_BOND (86)
 * Delegable: Delegation::NotDelegable
 * Amendment: featureProofOfParticipation
 * Privileges: NoPriv
 *
 * Immutable wrapper around STTx providing type-safe field access.
 * Use ValidatorBondBuilder to construct new transactions.
 */
class ValidatorBond : public TransactionBase
{
public:
    static constexpr xrpl::TxType txType = ttVALIDATOR_BOND;

    /**
     * @brief Construct a ValidatorBond transaction wrapper from an existing STTx object.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    explicit ValidatorBond(std::shared_ptr<STTx const> tx)
        : TransactionBase(std::move(tx))
    {
        // Verify transaction type
        if (tx_->getTxnType() != txType)
        {
            throw std::runtime_error("Invalid transaction type for ValidatorBond");
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

    /**
     * @brief Get sfBondedAmount (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_AMOUNT::type::value_type
    getBondedAmount() const
    {
        return this->tx_->at(sfBondedAmount);
    }
};

/**
 * @brief Builder for ValidatorBond transactions.
 *
 * Provides a fluent interface for constructing transactions with method chaining.
 * Uses STObject internally for flexible transaction construction.
 * Inherits common field setters from TransactionBuilderBase.
 */
class ValidatorBondBuilder : public TransactionBuilderBase<ValidatorBondBuilder>
{
public:
    /**
     * @brief Construct a new ValidatorBondBuilder with required fields.
     * @param account The account initiating the transaction.
     * @param consensusKey The sfConsensusKey field value.
     * @param bondedAmount The sfBondedAmount field value.
     * @param sequence Optional sequence number for the transaction.
     * @param fee Optional fee for the transaction.
     */
    ValidatorBondBuilder(SF_ACCOUNT::type::value_type account,
                     std::decay_t<typename SF_VL::type::value_type> const& consensusKey,                     std::decay_t<typename SF_AMOUNT::type::value_type> const& bondedAmount,                    std::optional<SF_UINT32::type::value_type> sequence = std::nullopt,
                    std::optional<SF_AMOUNT::type::value_type> fee = std::nullopt
)
        : TransactionBuilderBase<ValidatorBondBuilder>(ttVALIDATOR_BOND, account, sequence, fee)
    {
        setConsensusKey(consensusKey);
        setBondedAmount(bondedAmount);
    }

    /**
     * @brief Construct a ValidatorBondBuilder from an existing STTx object.
     * @param tx The existing transaction to copy from.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    ValidatorBondBuilder(std::shared_ptr<STTx const> tx)
    {
        if (tx->getTxnType() != ttVALIDATOR_BOND)
        {
            throw std::runtime_error("Invalid transaction type for ValidatorBondBuilder");
        }
        object_ = *tx;
    }

    /** @brief Transaction-specific field setters */

    /**
     * @brief Set sfConsensusKey (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setConsensusKey(std::decay_t<typename SF_VL::type::value_type> const& value)
    {
        object_[sfConsensusKey] = value;
        return *this;
    }

    /**
     * @brief Set sfBondedAmount (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setBondedAmount(std::decay_t<typename SF_AMOUNT::type::value_type> const& value)
    {
        object_[sfBondedAmount] = value;
        return *this;
    }

    /**
     * @brief Build and return the ValidatorBond wrapper.
     * @param publicKey The public key for signing.
     * @param secretKey The secret key for signing.
     * @return The constructed transaction wrapper.
     */
    ValidatorBond
    build(PublicKey const& publicKey, SecretKey const& secretKey)
    {
        sign(publicKey, secretKey);
        return ValidatorBond{std::make_shared<STTx>(std::move(object_))};
    }
};

}  // namespace xrpl::transactions
