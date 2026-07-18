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

class ClaimAmmLpRewardBuilder;

/**
 * @brief Transaction: ClaimAmmLpReward
 *
 * Type: ttCLAIM_AMM_LP_REWARD (94)
 * Delegable: Delegation::NotDelegable
 * Amendment: featureProofOfParticipation
 * Privileges: NoPriv
 *
 * Immutable wrapper around STTx providing type-safe field access.
 * Use ClaimAmmLpRewardBuilder to construct new transactions.
 */
class ClaimAmmLpReward : public TransactionBase
{
public:
    static constexpr xrpl::TxType txType = ttCLAIM_AMM_LP_REWARD;

    /**
     * @brief Construct a ClaimAmmLpReward transaction wrapper from an existing STTx object.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    explicit ClaimAmmLpReward(std::shared_ptr<STTx const> tx)
        : TransactionBase(std::move(tx))
    {
        if (tx_->getTxnType() != txType)
        {
            throw std::runtime_error("Invalid transaction type for ClaimAmmLpReward");
        }
    }

    [[nodiscard]]
    SF_ISSUE::type::value_type
    getAsset() const
    {
        return this->tx_->at(sfAsset);
    }

    [[nodiscard]]
    SF_ISSUE::type::value_type
    getAsset2() const
    {
        return this->tx_->at(sfAsset2);
    }
};

/**
 * @brief Builder for ClaimAmmLpReward transactions.
 */
class ClaimAmmLpRewardBuilder : public TransactionBuilderBase<ClaimAmmLpRewardBuilder>
{
public:
    ClaimAmmLpRewardBuilder(
        SF_ACCOUNT::type::value_type account,
        std::decay_t<typename SF_ISSUE::type::value_type> const& asset,
        std::decay_t<typename SF_ISSUE::type::value_type> const& asset2,
        std::optional<SF_UINT32::type::value_type> sequence = std::nullopt,
        std::optional<SF_AMOUNT::type::value_type> fee = std::nullopt)
        : TransactionBuilderBase<ClaimAmmLpRewardBuilder>(
              ttCLAIM_AMM_LP_REWARD,
              account,
              sequence,
              fee)
    {
        setAsset(asset);
        setAsset2(asset2);
    }

    ClaimAmmLpRewardBuilder(std::shared_ptr<STTx const> tx)
    {
        if (tx->getTxnType() != ttCLAIM_AMM_LP_REWARD)
        {
            throw std::runtime_error("Invalid transaction type for ClaimAmmLpRewardBuilder");
        }
        object_ = *tx;
    }

    ClaimAmmLpRewardBuilder&
    setAsset(std::decay_t<typename SF_ISSUE::type::value_type> const& value)
    {
        object_[sfAsset] = value;
        return *this;
    }

    ClaimAmmLpRewardBuilder&
    setAsset2(std::decay_t<typename SF_ISSUE::type::value_type> const& value)
    {
        object_[sfAsset2] = value;
        return *this;
    }

    ClaimAmmLpReward
    build(PublicKey const& publicKey, SecretKey const& secretKey)
    {
        sign(publicKey, secretKey);
        return ClaimAmmLpReward{std::make_shared<STTx>(std::move(object_))};
    }
};

}  // namespace xrpl::transactions
