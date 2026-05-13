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

class ClaimRewardBuilder;

/**
 * @brief Transaction: ClaimReward
 *
 * Type: ttCLAIM_REWARD (88)
 * Delegable: Delegation::NotDelegable
 * Amendment: featureProofOfParticipation
 * Privileges: NoPriv
 *
 * Immutable wrapper around STTx providing type-safe field access.
 * Use ClaimRewardBuilder to construct new transactions.
 */
class ClaimReward : public TransactionBase
{
public:
    static constexpr xrpl::TxType txType = ttCLAIM_REWARD;

    /**
     * @brief Construct a ClaimReward transaction wrapper from an existing STTx object.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    explicit ClaimReward(std::shared_ptr<STTx const> tx)
        : TransactionBase(std::move(tx))
    {
        // Verify transaction type
        if (tx_->getTxnType() != txType)
        {
            throw std::runtime_error("Invalid transaction type for ClaimReward");
        }
    }

    // Transaction-specific field getters
};

/**
 * @brief Builder for ClaimReward transactions.
 *
 * Provides a fluent interface for constructing transactions with method chaining.
 * Uses STObject internally for flexible transaction construction.
 * Inherits common field setters from TransactionBuilderBase.
 */
class ClaimRewardBuilder : public TransactionBuilderBase<ClaimRewardBuilder>
{
public:
    /**
     * @brief Construct a new ClaimRewardBuilder with required fields.
     * @param account The account initiating the transaction.
     * @param sequence Optional sequence number for the transaction.
     * @param fee Optional fee for the transaction.
     */
    ClaimRewardBuilder(SF_ACCOUNT::type::value_type account,
                    std::optional<SF_UINT32::type::value_type> sequence = std::nullopt,
                    std::optional<SF_AMOUNT::type::value_type> fee = std::nullopt
)
        : TransactionBuilderBase<ClaimRewardBuilder>(ttCLAIM_REWARD, account, sequence, fee)
    {
    }

    /**
     * @brief Construct a ClaimRewardBuilder from an existing STTx object.
     * @param tx The existing transaction to copy from.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    ClaimRewardBuilder(std::shared_ptr<STTx const> tx)
    {
        if (tx->getTxnType() != ttCLAIM_REWARD)
        {
            throw std::runtime_error("Invalid transaction type for ClaimRewardBuilder");
        }
        object_ = *tx;
    }

    /** @brief Transaction-specific field setters */

    /**
     * @brief Build and return the ClaimReward wrapper.
     * @param publicKey The public key for signing.
     * @param secretKey The secret key for signing.
     * @return The constructed transaction wrapper.
     */
    ClaimReward
    build(PublicKey const& publicKey, SecretKey const& secretKey)
    {
        sign(publicKey, secretKey);
        return ClaimReward{std::make_shared<STTx>(std::move(object_))};
    }
};

}  // namespace xrpl::transactions
