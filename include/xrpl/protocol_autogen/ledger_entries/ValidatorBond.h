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

class ValidatorBondBuilder;

/**
 * @brief Ledger Entry: ValidatorBond
 *
 * Type: ltVALIDATOR_BOND (0x0091)
 * RPC Name: validator_bond
 *
 * Immutable wrapper around SLE providing type-safe field access.
 * Use ValidatorBondBuilder to construct new ledger entries.
 */
class ValidatorBond : public LedgerEntryBase
{
public:
    static constexpr LedgerEntryType entryType = ltVALIDATOR_BOND;

    /**
     * @brief Construct a ValidatorBond ledger entry wrapper from an existing SLE object.
     * @throws std::runtime_error if the ledger entry type doesn't match.
     */
    explicit ValidatorBond(std::shared_ptr<SLE const> sle)
        : LedgerEntryBase(std::move(sle))
    {
        // Verify ledger entry type
        if (sle_->getType() != entryType)
        {
            throw std::runtime_error("Invalid ledger entry type for ValidatorBond");
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
     * @brief Get sfPublicKey (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_VL::type::value_type
    getPublicKey() const
    {
        return this->sle_->at(sfPublicKey);
    }

    /**
     * @brief Get sfConsensusKey (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_VL::type::value_type
    getConsensusKey() const
    {
        return this->sle_->at(sfConsensusKey);
    }

    /**
     * @brief Get sfBondedAmount (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_AMOUNT::type::value_type
    getBondedAmount() const
    {
        return this->sle_->at(sfBondedAmount);
    }

    /**
     * @brief Get sfBondStatus (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getBondStatus() const
    {
        return this->sle_->at(sfBondStatus);
    }

    /**
     * @brief Get sfUptimeBps (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getUptimeBps() const
    {
        if (hasUptimeBps())
            return this->sle_->at(sfUptimeBps);
        return std::nullopt;
    }

    /**
     * @brief Check if sfUptimeBps is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasUptimeBps() const
    {
        return this->sle_->isFieldPresent(sfUptimeBps);
    }

    /**
     * @brief Get sfVoteAccuracyBps (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getVoteAccuracyBps() const
    {
        if (hasVoteAccuracyBps())
            return this->sle_->at(sfVoteAccuracyBps);
        return std::nullopt;
    }

    /**
     * @brief Check if sfVoteAccuracyBps is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasVoteAccuracyBps() const
    {
        return this->sle_->isFieldPresent(sfVoteAccuracyBps);
    }

    /**
     * @brief Get sfLatencyScoreBps (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getLatencyScoreBps() const
    {
        if (hasLatencyScoreBps())
            return this->sle_->at(sfLatencyScoreBps);
        return std::nullopt;
    }

    /**
     * @brief Check if sfLatencyScoreBps is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasLatencyScoreBps() const
    {
        return this->sle_->isFieldPresent(sfLatencyScoreBps);
    }

    /**
     * @brief Get sfConsistencyBps (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getConsistencyBps() const
    {
        if (hasConsistencyBps())
            return this->sle_->at(sfConsistencyBps);
        return std::nullopt;
    }

    /**
     * @brief Check if sfConsistencyBps is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasConsistencyBps() const
    {
        return this->sle_->isFieldPresent(sfConsistencyBps);
    }

    /**
     * @brief Get sfSlashMultiplier (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getSlashMultiplier() const
    {
        return this->sle_->at(sfSlashMultiplier);
    }

    /**
     * @brief Get sfCompositeScore (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getCompositeScore() const
    {
        if (hasCompositeScore())
            return this->sle_->at(sfCompositeScore);
        return std::nullopt;
    }

    /**
     * @brief Check if sfCompositeScore is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasCompositeScore() const
    {
        return this->sle_->isFieldPresent(sfCompositeScore);
    }

    /**
     * @brief Get sfSlashCount (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getSlashCount() const
    {
        if (hasSlashCount())
            return this->sle_->at(sfSlashCount);
        return std::nullopt;
    }

    /**
     * @brief Check if sfSlashCount is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasSlashCount() const
    {
        return this->sle_->isFieldPresent(sfSlashCount);
    }

    /**
     * @brief Get sfLastClaimedEpoch (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getLastClaimedEpoch() const
    {
        if (hasLastClaimedEpoch())
            return this->sle_->at(sfLastClaimedEpoch);
        return std::nullopt;
    }

    /**
     * @brief Check if sfLastClaimedEpoch is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasLastClaimedEpoch() const
    {
        return this->sle_->isFieldPresent(sfLastClaimedEpoch);
    }

    /**
     * @brief Get sfUnbondingStartLedger (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getUnbondingStartLedger() const
    {
        if (hasUnbondingStartLedger())
            return this->sle_->at(sfUnbondingStartLedger);
        return std::nullopt;
    }

    /**
     * @brief Check if sfUnbondingStartLedger is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasUnbondingStartLedger() const
    {
        return this->sle_->isFieldPresent(sfUnbondingStartLedger);
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
 * @brief Builder for ValidatorBond ledger entries.
 *
 * Provides a fluent interface for constructing ledger entries with method chaining.
 * Uses STObject internally for flexible ledger entry construction.
 * Inherits common field setters from LedgerEntryBuilderBase.
 */
class ValidatorBondBuilder : public LedgerEntryBuilderBase<ValidatorBondBuilder>
{
public:
    /**
     * @brief Construct a new ValidatorBondBuilder with required fields.
     * @param account The sfAccount field value.
     * @param publicKey The sfPublicKey field value.
     * @param consensusKey The sfConsensusKey field value.
     * @param bondedAmount The sfBondedAmount field value.
     * @param bondStatus The sfBondStatus field value.
     * @param slashMultiplier The sfSlashMultiplier field value.
     * @param ownerNode The sfOwnerNode field value.
     * @param previousTxnID The sfPreviousTxnID field value.
     * @param previousTxnLgrSeq The sfPreviousTxnLgrSeq field value.
     */
    ValidatorBondBuilder(std::decay_t<typename SF_ACCOUNT::type::value_type> const& account,std::decay_t<typename SF_VL::type::value_type> const& publicKey,std::decay_t<typename SF_VL::type::value_type> const& consensusKey,std::decay_t<typename SF_AMOUNT::type::value_type> const& bondedAmount,std::decay_t<typename SF_UINT32::type::value_type> const& bondStatus,std::decay_t<typename SF_UINT32::type::value_type> const& slashMultiplier,std::decay_t<typename SF_UINT64::type::value_type> const& ownerNode,std::decay_t<typename SF_UINT256::type::value_type> const& previousTxnID,std::decay_t<typename SF_UINT32::type::value_type> const& previousTxnLgrSeq)
        : LedgerEntryBuilderBase<ValidatorBondBuilder>(ltVALIDATOR_BOND)
    {
        setAccount(account);
        setPublicKey(publicKey);
        setConsensusKey(consensusKey);
        setBondedAmount(bondedAmount);
        setBondStatus(bondStatus);
        setSlashMultiplier(slashMultiplier);
        setOwnerNode(ownerNode);
        setPreviousTxnID(previousTxnID);
        setPreviousTxnLgrSeq(previousTxnLgrSeq);
    }

    /**
     * @brief Construct a ValidatorBondBuilder from an existing SLE object.
     * @param sle The existing ledger entry to copy from.
     * @throws std::runtime_error if the ledger entry type doesn't match.
     */
    ValidatorBondBuilder(std::shared_ptr<SLE const> sle)
    {
        if (sle->at(sfLedgerEntryType) != ltVALIDATOR_BOND)
        {
            throw std::runtime_error("Invalid ledger entry type for ValidatorBond");
        }
        object_ = *sle;
    }

    /** @brief Ledger entry-specific field setters */

    /**
     * @brief Set sfAccount (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setAccount(std::decay_t<typename SF_ACCOUNT::type::value_type> const& value)
    {
        object_[sfAccount] = value;
        return *this;
    }

    /**
     * @brief Set sfPublicKey (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setPublicKey(std::decay_t<typename SF_VL::type::value_type> const& value)
    {
        object_[sfPublicKey] = value;
        return *this;
    }

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
     * @brief Set sfBondStatus (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setBondStatus(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfBondStatus] = value;
        return *this;
    }

    /**
     * @brief Set sfUptimeBps (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setUptimeBps(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfUptimeBps] = value;
        return *this;
    }

    /**
     * @brief Set sfVoteAccuracyBps (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setVoteAccuracyBps(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfVoteAccuracyBps] = value;
        return *this;
    }

    /**
     * @brief Set sfLatencyScoreBps (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setLatencyScoreBps(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfLatencyScoreBps] = value;
        return *this;
    }

    /**
     * @brief Set sfConsistencyBps (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setConsistencyBps(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfConsistencyBps] = value;
        return *this;
    }

    /**
     * @brief Set sfSlashMultiplier (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setSlashMultiplier(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfSlashMultiplier] = value;
        return *this;
    }

    /**
     * @brief Set sfCompositeScore (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setCompositeScore(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfCompositeScore] = value;
        return *this;
    }

    /**
     * @brief Set sfSlashCount (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setSlashCount(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfSlashCount] = value;
        return *this;
    }

    /**
     * @brief Set sfLastClaimedEpoch (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setLastClaimedEpoch(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfLastClaimedEpoch] = value;
        return *this;
    }

    /**
     * @brief Set sfUnbondingStartLedger (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setUnbondingStartLedger(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfUnbondingStartLedger] = value;
        return *this;
    }

    /**
     * @brief Set sfOwnerNode (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setOwnerNode(std::decay_t<typename SF_UINT64::type::value_type> const& value)
    {
        object_[sfOwnerNode] = value;
        return *this;
    }

    /**
     * @brief Set sfPreviousTxnID (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setPreviousTxnID(std::decay_t<typename SF_UINT256::type::value_type> const& value)
    {
        object_[sfPreviousTxnID] = value;
        return *this;
    }

    /**
     * @brief Set sfPreviousTxnLgrSeq (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    ValidatorBondBuilder&
    setPreviousTxnLgrSeq(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfPreviousTxnLgrSeq] = value;
        return *this;
    }

    /**
     * @brief Build and return the completed ValidatorBond wrapper.
     * @param index The ledger entry index.
     * @return The constructed ledger entry wrapper.
     */
    ValidatorBond
    build(uint256 const& index)
    {
        return ValidatorBond{std::make_shared<SLE>(std::move(object_), index)};
    }
};

}  // namespace xrpl::ledger_entries
