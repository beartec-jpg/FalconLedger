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

class GovernanceVoteBuilder;

/**
 * @brief Transaction: GovernanceVote
 *
 * Type: ttGOVERNANCE_VOTE (92)
 * Delegable: Delegation::NotDelegable
 * Amendment: featureProofOfParticipation
 * Privileges: NoPriv
 *
 * Immutable wrapper around STTx providing type-safe field access.
 * Use GovernanceVoteBuilder to construct new transactions.
 */
class GovernanceVote : public TransactionBase
{
public:
    static constexpr xrpl::TxType txType = ttGOVERNANCE_VOTE;

    /**
     * @brief Construct a GovernanceVote transaction wrapper from an existing STTx object.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    explicit GovernanceVote(std::shared_ptr<STTx const> tx)
        : TransactionBase(std::move(tx))
    {
        // Verify transaction type
        if (tx_->getTxnType() != txType)
        {
            throw std::runtime_error("Invalid transaction type for GovernanceVote");
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
     * @brief Get sfProposalID (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT256::type::value_type
    getProposalID() const
    {
        return this->tx_->at(sfProposalID);
    }

    /**
     * @brief Get sfVoteWeight (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getVoteWeight() const
    {
        return this->tx_->at(sfVoteWeight);
    }
};

/**
 * @brief Builder for GovernanceVote transactions.
 *
 * Provides a fluent interface for constructing transactions with method chaining.
 * Uses STObject internally for flexible transaction construction.
 * Inherits common field setters from TransactionBuilderBase.
 */
class GovernanceVoteBuilder : public TransactionBuilderBase<GovernanceVoteBuilder>
{
public:
    /**
     * @brief Construct a new GovernanceVoteBuilder with required fields.
     * @param account The account initiating the transaction.
     * @param consensusKey The sfConsensusKey field value.
     * @param proposalID The sfProposalID field value.
     * @param voteWeight The sfVoteWeight field value.
     * @param sequence Optional sequence number for the transaction.
     * @param fee Optional fee for the transaction.
     */
    GovernanceVoteBuilder(SF_ACCOUNT::type::value_type account,
                     std::decay_t<typename SF_VL::type::value_type> const& consensusKey,                     std::decay_t<typename SF_UINT256::type::value_type> const& proposalID,                     std::decay_t<typename SF_UINT32::type::value_type> const& voteWeight,                    std::optional<SF_UINT32::type::value_type> sequence = std::nullopt,
                    std::optional<SF_AMOUNT::type::value_type> fee = std::nullopt
)
        : TransactionBuilderBase<GovernanceVoteBuilder>(ttGOVERNANCE_VOTE, account, sequence, fee)
    {
        setConsensusKey(consensusKey);
        setProposalID(proposalID);
        setVoteWeight(voteWeight);
    }

    /**
     * @brief Construct a GovernanceVoteBuilder from an existing STTx object.
     * @param tx The existing transaction to copy from.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    GovernanceVoteBuilder(std::shared_ptr<STTx const> tx)
    {
        if (tx->getTxnType() != ttGOVERNANCE_VOTE)
        {
            throw std::runtime_error("Invalid transaction type for GovernanceVoteBuilder");
        }
        object_ = *tx;
    }

    /** @brief Transaction-specific field setters */

    /**
     * @brief Set sfConsensusKey (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceVoteBuilder&
    setConsensusKey(std::decay_t<typename SF_VL::type::value_type> const& value)
    {
        object_[sfConsensusKey] = value;
        return *this;
    }

    /**
     * @brief Set sfProposalID (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceVoteBuilder&
    setProposalID(std::decay_t<typename SF_UINT256::type::value_type> const& value)
    {
        object_[sfProposalID] = value;
        return *this;
    }

    /**
     * @brief Set sfVoteWeight (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceVoteBuilder&
    setVoteWeight(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfVoteWeight] = value;
        return *this;
    }

    /**
     * @brief Build and return the GovernanceVote wrapper.
     * @param publicKey The public key for signing.
     * @param secretKey The secret key for signing.
     * @return The constructed transaction wrapper.
     */
    GovernanceVote
    build(PublicKey const& publicKey, SecretKey const& secretKey)
    {
        sign(publicKey, secretKey);
        return GovernanceVote{std::make_shared<STTx>(std::move(object_))};
    }
};

}  // namespace xrpl::transactions
