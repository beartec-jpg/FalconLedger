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

class ValidatorSlashBuilder;

/**
 * @brief Transaction: ValidatorSlash
 *
 * Type: ttVALIDATOR_SLASH (89)
 * Delegable: Delegation::NotDelegable
 * Amendment: featureProofOfParticipation
 * Privileges: NoPriv
 *
 * Immutable wrapper around STTx providing type-safe field access.
 * Use ValidatorSlashBuilder to construct new transactions.
 */
class ValidatorSlash : public TransactionBase
{
public:
    static constexpr xrpl::TxType txType = ttVALIDATOR_SLASH;

    /**
     * @brief Construct a ValidatorSlash transaction wrapper from an existing STTx object.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    explicit ValidatorSlash(std::shared_ptr<STTx const> tx)
        : TransactionBase(std::move(tx))
    {
        // Verify transaction type
        if (tx_->getTxnType() != txType)
        {
            throw std::runtime_error("Invalid transaction type for ValidatorSlash");
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

    /**
     * @brief Get sfSlashOffense (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getSlashOffense() const
    {
        return this->tx_->at(sfSlashOffense);
    }

    /**
     * @brief Get sfSlashEvidence1 (SoeOptional)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_VL::type::value_type>
    getSlashEvidence1() const
    {
        if (hasSlashEvidence1())
        {
            return this->tx_->at(sfSlashEvidence1);
        }
        return std::nullopt;
    }

    /**
     * @brief Check if sfSlashEvidence1 is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasSlashEvidence1() const
    {
        return this->tx_->isFieldPresent(sfSlashEvidence1);
    }

    /**
     * @brief Get sfSlashEvidence2 (SoeOptional)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_VL::type::value_type>
    getSlashEvidence2() const
    {
        if (hasSlashEvidence2())
        {
            return this->tx_->at(sfSlashEvidence2);
        }
        return std::nullopt;
    }

    /**
     * @brief Check if sfSlashEvidence2 is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasSlashEvidence2() const
    {
        return this->tx_->isFieldPresent(sfSlashEvidence2);
    }
};

/**
 * @brief Builder for ValidatorSlash transactions.
 *
 * Provides a fluent interface for constructing transactions with method chaining.
 * Uses STObject internally for flexible transaction construction.
 * Inherits common field setters from TransactionBuilderBase.
 */
class ValidatorSlashBuilder : public TransactionBuilderBase<ValidatorSlashBuilder>
{
public:
    /**
     * @brief Construct a new ValidatorSlashBuilder with required fields.
     * @param account The account initiating the transaction.
     * @param slashTarget The sfSlashTarget field value.
     * @param slashOffense The sfSlashOffense field value.
     * @param sequence Optional sequence number for the transaction.
     * @param fee Optional fee for the transaction.
     */
    ValidatorSlashBuilder(SF_ACCOUNT::type::value_type account,
                     std::decay_t<typename SF_ACCOUNT::type::value_type> const& slashTarget,                     std::decay_t<typename SF_UINT32::type::value_type> const& slashOffense,                    std::optional<SF_UINT32::type::value_type> sequence = std::nullopt,
                    std::optional<SF_AMOUNT::type::value_type> fee = std::nullopt
)
        : TransactionBuilderBase<ValidatorSlashBuilder>(ttVALIDATOR_SLASH, account, sequence, fee)
    {
        setSlashTarget(slashTarget);
        setSlashOffense(slashOffense);
    }

    /**
     * @brief Construct a ValidatorSlashBuilder from an existing STTx object.
     * @param tx The existing transaction to copy from.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    ValidatorSlashBuilder(std::shared_ptr<STTx const> tx)
    {
        if (tx->getTxnType() != ttVALIDATOR_SLASH)
        {
            throw std::runtime_error("Invalid transaction type for ValidatorSlashBuilder");
        }
        object_ = *tx;
    }

    /** @brief Transaction-specific field setters */

    /**
     * @brief Set sfSlashTarget (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorSlashBuilder&
    setSlashTarget(std::decay_t<typename SF_ACCOUNT::type::value_type> const& value)
    {
        object_[sfSlashTarget] = value;
        return *this;
    }

    /**
     * @brief Set sfSlashOffense (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorSlashBuilder&
    setSlashOffense(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfSlashOffense] = value;
        return *this;
    }

    /**
     * @brief Set sfSlashEvidence1 (SoeOptional)
     * @return Reference to this builder for method chaining.
     */
    ValidatorSlashBuilder&
    setSlashEvidence1(std::decay_t<typename SF_VL::type::value_type> const& value)
    {
        object_[sfSlashEvidence1] = value;
        return *this;
    }

    /**
     * @brief Set sfSlashEvidence2 (SoeOptional)
     * @return Reference to this builder for method chaining.
     */
    ValidatorSlashBuilder&
    setSlashEvidence2(std::decay_t<typename SF_VL::type::value_type> const& value)
    {
        object_[sfSlashEvidence2] = value;
        return *this;
    }

    /**
     * @brief Build and return the ValidatorSlash wrapper.
     * @param publicKey The public key for signing.
     * @param secretKey The secret key for signing.
     * @return The constructed transaction wrapper.
     */
    ValidatorSlash
    build(PublicKey const& publicKey, SecretKey const& secretKey)
    {
        sign(publicKey, secretKey);
        return ValidatorSlash{std::make_shared<STTx>(std::move(object_))};
    }
};

}  // namespace xrpl::transactions
