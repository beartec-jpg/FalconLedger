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

class ClaimLPRewardBuilder;

/**
 * @brief Transaction: ClaimLPReward
 *
 * Type: ttCLAIM_LP_REWARD (93)
 * Delegable: Delegation::NotDelegable
 * Amendment: featureProofOfParticipation
 * Privileges: NoPriv
 *
 * Immutable wrapper around STTx providing type-safe field access.
 * Use ClaimLPRewardBuilder to construct new transactions.
 */
class ClaimLPReward : public TransactionBase
{
public:
    static constexpr xrpl::TxType txType = ttCLAIM_LP_REWARD;

    /**
     * @brief Construct a ClaimLPReward transaction wrapper from an existing STTx object.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    explicit ClaimLPReward(std::shared_ptr<STTx const> tx)
        : TransactionBase(std::move(tx))
    {
        // Verify transaction type
        if (tx_->getTxnType() != txType)
        {
            throw std::runtime_error("Invalid transaction type for ClaimLPReward");
        }
    }

    // Transaction-specific field getters

    /**
     * @brief Get sfVaultID (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT256::type::value_type
    getVaultID() const
    {
        return this->tx_->at(sfVaultID);
    }
};

/**
 * @brief Builder for ClaimLPReward transactions.
 *
 * Provides a fluent interface for constructing transactions with method chaining.
 * Uses STObject internally for flexible transaction construction.
 * Inherits common field setters from TransactionBuilderBase.
 */
class ClaimLPRewardBuilder : public TransactionBuilderBase<ClaimLPRewardBuilder>
{
public:
    /**
     * @brief Construct a new ClaimLPRewardBuilder with required fields.
     * @param account The account initiating the transaction.
     * @param vaultID The sfVaultID field value.
     * @param sequence Optional sequence number for the transaction.
     * @param fee Optional fee for the transaction.
     */
    ClaimLPRewardBuilder(SF_ACCOUNT::type::value_type account,
                     std::decay_t<typename SF_UINT256::type::value_type> const& vaultID,                    std::optional<SF_UINT32::type::value_type> sequence = std::nullopt,
                    std::optional<SF_AMOUNT::type::value_type> fee = std::nullopt
)
        : TransactionBuilderBase<ClaimLPRewardBuilder>(ttCLAIM_LP_REWARD, account, sequence, fee)
    {
        setVaultID(vaultID);
    }

    /**
     * @brief Construct a ClaimLPRewardBuilder from an existing STTx object.
     * @param tx The existing transaction to copy from.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    ClaimLPRewardBuilder(std::shared_ptr<STTx const> tx)
    {
        if (tx->getTxnType() != ttCLAIM_LP_REWARD)
        {
            throw std::runtime_error("Invalid transaction type for ClaimLPRewardBuilder");
        }
        object_ = *tx;
    }

    /** @brief Transaction-specific field setters */

    /**
     * @brief Set sfVaultID (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ClaimLPRewardBuilder&
    setVaultID(std::decay_t<typename SF_UINT256::type::value_type> const& value)
    {
        object_[sfVaultID] = value;
        return *this;
    }

    /**
     * @brief Build and return the ClaimLPReward wrapper.
     * @param publicKey The public key for signing.
     * @param secretKey The secret key for signing.
     * @return The constructed transaction wrapper.
     */
    ClaimLPReward
    build(PublicKey const& publicKey, SecretKey const& secretKey)
    {
        sign(publicKey, secretKey);
        return ClaimLPReward{std::make_shared<STTx>(std::move(object_))};
    }
};

}  // namespace xrpl::transactions
