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

class GovernanceProposalBuilder;

/**
 * @brief Transaction: GovernanceProposal
 *
 * Type: ttGOVERNANCE_PROPOSAL (91)
 * Delegable: Delegation::NotDelegable
 * Amendment: featureProofOfParticipation
 * Privileges: NoPriv
 *
 * Immutable wrapper around STTx providing type-safe field access.
 * Use GovernanceProposalBuilder to construct new transactions.
 */
class GovernanceProposal : public TransactionBase
{
public:
    static constexpr xrpl::TxType txType = ttGOVERNANCE_PROPOSAL;

    /**
     * @brief Construct a GovernanceProposal transaction wrapper from an existing STTx object.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    explicit GovernanceProposal(std::shared_ptr<STTx const> tx)
        : TransactionBase(std::move(tx))
    {
        // Verify transaction type
        if (tx_->getTxnType() != txType)
        {
            throw std::runtime_error("Invalid transaction type for GovernanceProposal");
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
     * @brief Get sfProposalType (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getProposalType() const
    {
        return this->tx_->at(sfProposalType);
    }

    /**
     * @brief Get sfProposalValue (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getProposalValue() const
    {
        return this->tx_->at(sfProposalValue);
    }
};

/**
 * @brief Builder for GovernanceProposal transactions.
 *
 * Provides a fluent interface for constructing transactions with method chaining.
 * Uses STObject internally for flexible transaction construction.
 * Inherits common field setters from TransactionBuilderBase.
 */
class GovernanceProposalBuilder : public TransactionBuilderBase<GovernanceProposalBuilder>
{
public:
    /**
     * @brief Construct a new GovernanceProposalBuilder with required fields.
     * @param account The account initiating the transaction.
     * @param consensusKey The sfConsensusKey field value.
     * @param proposalType The sfProposalType field value.
     * @param proposalValue The sfProposalValue field value.
     * @param sequence Optional sequence number for the transaction.
     * @param fee Optional fee for the transaction.
     */
    GovernanceProposalBuilder(SF_ACCOUNT::type::value_type account,
                     std::decay_t<typename SF_VL::type::value_type> const& consensusKey,                     std::decay_t<typename SF_UINT32::type::value_type> const& proposalType,                     std::decay_t<typename SF_UINT32::type::value_type> const& proposalValue,                    std::optional<SF_UINT32::type::value_type> sequence = std::nullopt,
                    std::optional<SF_AMOUNT::type::value_type> fee = std::nullopt
)
        : TransactionBuilderBase<GovernanceProposalBuilder>(ttGOVERNANCE_PROPOSAL, account, sequence, fee)
    {
        setConsensusKey(consensusKey);
        setProposalType(proposalType);
        setProposalValue(proposalValue);
    }

    /**
     * @brief Construct a GovernanceProposalBuilder from an existing STTx object.
     * @param tx The existing transaction to copy from.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    GovernanceProposalBuilder(std::shared_ptr<STTx const> tx)
    {
        if (tx->getTxnType() != ttGOVERNANCE_PROPOSAL)
        {
            throw std::runtime_error("Invalid transaction type for GovernanceProposalBuilder");
        }
        object_ = *tx;
    }

    /** @brief Transaction-specific field setters */

    /**
     * @brief Set sfConsensusKey (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setConsensusKey(std::decay_t<typename SF_VL::type::value_type> const& value)
    {
        object_[sfConsensusKey] = value;
        return *this;
    }

    /**
     * @brief Set sfProposalType (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setProposalType(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfProposalType] = value;
        return *this;
    }

    /**
     * @brief Set sfProposalValue (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setProposalValue(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfProposalValue] = value;
        return *this;
    }

    /**
     * @brief Build and return the GovernanceProposal wrapper.
     * @param publicKey The public key for signing.
     * @param secretKey The secret key for signing.
     * @return The constructed transaction wrapper.
     */
    GovernanceProposal
    build(PublicKey const& publicKey, SecretKey const& secretKey)
    {
        sign(publicKey, secretKey);
        return GovernanceProposal{std::make_shared<STTx>(std::move(object_))};
    }
};

}  // namespace xrpl::transactions
