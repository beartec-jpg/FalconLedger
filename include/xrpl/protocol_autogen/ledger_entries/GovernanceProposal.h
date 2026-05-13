// This file is auto-generated. Do not edit.
#pragma once

#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STParsedJSON.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol_autogen/LedgerEntryBase.h>
#include <xrpl/protocol_autogen/LedgerEntryBuilderBase.h>
#include <xrpl/json/json_value.h>

#include <stdexcept>
#include <optional>

namespace xrpl::ledger_entries {

class GovernanceProposalBuilder;

/**
 * @brief Ledger Entry: GovernanceProposal
 *
 * Type: ltGOVERNANCE_PROPOSAL (0x0094)
 * RPC Name: governance_proposal
 *
 * Immutable wrapper around SLE providing type-safe field access.
 * Use GovernanceProposalBuilder to construct new ledger entries.
 */
class GovernanceProposal : public LedgerEntryBase
{
public:
    static constexpr LedgerEntryType entryType = ltGOVERNANCE_PROPOSAL;

    /**
     * @brief Construct a GovernanceProposal ledger entry wrapper from an existing SLE object.
     * @throws std::runtime_error if the ledger entry type doesn't match.
     */
    explicit GovernanceProposal(std::shared_ptr<SLE const> sle)
        : LedgerEntryBase(std::move(sle))
    {
        // Verify ledger entry type
        if (sle_->getType() != entryType)
        {
            throw std::runtime_error("Invalid ledger entry type for GovernanceProposal");
        }
    }

    // Ledger entry-specific field getters

    /**
     * @brief Get sfAccount (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_ACCOUNT::type::value_type
    getAccount() const
    {
        return this->sle_->at(sfAccount);
    }

    /**
     * @brief Get sfProposalType (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getProposalType() const
    {
        return this->sle_->at(sfProposalType);
    }

    /**
     * @brief Get sfProposalValue (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getProposalValue() const
    {
        return this->sle_->at(sfProposalValue);
    }

    /**
     * @brief Get sfProposalExpiry (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getProposalExpiry() const
    {
        return this->sle_->at(sfProposalExpiry);
    }

    /**
     * @brief Get sfProposalState (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getProposalState() const
    {
        return this->sle_->at(sfProposalState);
    }

    /**
     * @brief Get sfVotedFor (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getVotedFor() const
    {
        if (hasVotedFor())
            return this->sle_->at(sfVotedFor);
        return std::nullopt;
    }

    /**
     * @brief Check if sfVotedFor is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasVotedFor() const
    {
        return this->sle_->isFieldPresent(sfVotedFor);
    }

    /**
     * @brief Get sfVotedAgainst (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getVotedAgainst() const
    {
        if (hasVotedAgainst())
            return this->sle_->at(sfVotedAgainst);
        return std::nullopt;
    }

    /**
     * @brief Check if sfVotedAgainst is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasVotedAgainst() const
    {
        return this->sle_->isFieldPresent(sfVotedAgainst);
    }

    /**
     * @brief Get sfVoterList (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_VL::type::value_type>
    getVoterList() const
    {
        if (hasVoterList())
            return this->sle_->at(sfVoterList);
        return std::nullopt;
    }

    /**
     * @brief Check if sfVoterList is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasVoterList() const
    {
        return this->sle_->isFieldPresent(sfVoterList);
    }

    /**
     * @brief Get sfOwnerNode (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT64::type::value_type
    getOwnerNode() const
    {
        return this->sle_->at(sfOwnerNode);
    }

    /**
     * @brief Get sfPreviousTxnID (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT256::type::value_type
    getPreviousTxnID() const
    {
        return this->sle_->at(sfPreviousTxnID);
    }

    /**
     * @brief Get sfPreviousTxnLgrSeq (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getPreviousTxnLgrSeq() const
    {
        return this->sle_->at(sfPreviousTxnLgrSeq);
    }
};

/**
 * @brief Builder for GovernanceProposal ledger entries.
 *
 * Provides a fluent interface for constructing ledger entries with method chaining.
 * Uses STObject internally for flexible ledger entry construction.
 * Inherits common field setters from LedgerEntryBuilderBase.
 */
class GovernanceProposalBuilder : public LedgerEntryBuilderBase<GovernanceProposalBuilder>
{
public:
    /**
     * @brief Construct a new GovernanceProposalBuilder with required fields.
     * @param account The sfAccount field value.
     * @param proposalType The sfProposalType field value.
     * @param proposalValue The sfProposalValue field value.
     * @param proposalExpiry The sfProposalExpiry field value.
     * @param proposalState The sfProposalState field value.
     * @param ownerNode The sfOwnerNode field value.
     * @param previousTxnID The sfPreviousTxnID field value.
     * @param previousTxnLgrSeq The sfPreviousTxnLgrSeq field value.
     */
    GovernanceProposalBuilder(std::decay_t<typename SF_ACCOUNT::type::value_type> const& account,std::decay_t<typename SF_UINT32::type::value_type> const& proposalType,std::decay_t<typename SF_UINT32::type::value_type> const& proposalValue,std::decay_t<typename SF_UINT32::type::value_type> const& proposalExpiry,std::decay_t<typename SF_UINT32::type::value_type> const& proposalState,std::decay_t<typename SF_UINT64::type::value_type> const& ownerNode,std::decay_t<typename SF_UINT256::type::value_type> const& previousTxnID,std::decay_t<typename SF_UINT32::type::value_type> const& previousTxnLgrSeq)
        : LedgerEntryBuilderBase<GovernanceProposalBuilder>(ltGOVERNANCE_PROPOSAL)
    {
        setAccount(account);
        setProposalType(proposalType);
        setProposalValue(proposalValue);
        setProposalExpiry(proposalExpiry);
        setProposalState(proposalState);
        setOwnerNode(ownerNode);
        setPreviousTxnID(previousTxnID);
        setPreviousTxnLgrSeq(previousTxnLgrSeq);
    }

    /**
     * @brief Construct a GovernanceProposalBuilder from an existing SLE object.
     * @param sle The existing ledger entry to copy from.
     * @throws std::runtime_error if the ledger entry type doesn't match.
     */
    GovernanceProposalBuilder(std::shared_ptr<SLE const> sle)
    {
        if (sle->at(sfLedgerEntryType) != ltGOVERNANCE_PROPOSAL)
        {
            throw std::runtime_error("Invalid ledger entry type for GovernanceProposal");
        }
        object_ = *sle;
    }

    /** @brief Ledger entry-specific field setters */

    /**
     * @brief Set sfAccount (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setAccount(std::decay_t<typename SF_ACCOUNT::type::value_type> const& value)
    {
        object_[sfAccount] = value;
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
     * @brief Set sfProposalExpiry (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setProposalExpiry(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfProposalExpiry] = value;
        return *this;
    }

    /**
     * @brief Set sfProposalState (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setProposalState(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfProposalState] = value;
        return *this;
    }

    /**
     * @brief Set sfVotedFor (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setVotedFor(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfVotedFor] = value;
        return *this;
    }

    /**
     * @brief Set sfVotedAgainst (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setVotedAgainst(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfVotedAgainst] = value;
        return *this;
    }

    /**
     * @brief Set sfVoterList (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setVoterList(std::decay_t<typename SF_VL::type::value_type> const& value)
    {
        object_[sfVoterList] = value;
        return *this;
    }

    /**
     * @brief Set sfOwnerNode (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setOwnerNode(std::decay_t<typename SF_UINT64::type::value_type> const& value)
    {
        object_[sfOwnerNode] = value;
        return *this;
    }

    /**
     * @brief Set sfPreviousTxnID (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setPreviousTxnID(std::decay_t<typename SF_UINT256::type::value_type> const& value)
    {
        object_[sfPreviousTxnID] = value;
        return *this;
    }

    /**
     * @brief Set sfPreviousTxnLgrSeq (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceProposalBuilder&
    setPreviousTxnLgrSeq(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfPreviousTxnLgrSeq] = value;
        return *this;
    }

    /**
     * @brief Build and return the completed GovernanceProposal wrapper.
     * @param index The ledger entry index.
     * @return The constructed ledger entry wrapper.
     */
    GovernanceProposal
    build(uint256 const& index)
    {
        return GovernanceProposal{std::make_shared<SLE>(std::move(object_), index)};
    }
};

}  // namespace xrpl::ledger_entries
