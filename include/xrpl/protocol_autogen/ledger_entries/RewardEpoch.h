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

class RewardEpochBuilder;

/**
 * @brief Ledger Entry: RewardEpoch
 *
 * Type: ltREWARD_EPOCH (0x0092)
 * RPC Name: reward_epoch
 *
 * Immutable wrapper around SLE providing type-safe field access.
 * Use RewardEpochBuilder to construct new ledger entries.
 */
class RewardEpoch : public LedgerEntryBase
{
public:
    static constexpr LedgerEntryType entryType = ltREWARD_EPOCH;

    /**
     * @brief Construct a RewardEpoch ledger entry wrapper from an existing SLE object.
     * @throws std::runtime_error if the ledger entry type doesn't match.
     */
    explicit RewardEpoch(std::shared_ptr<SLE const> sle)
        : LedgerEntryBase(std::move(sle))
    {
        // Verify ledger entry type
        if (sle_->getType() != entryType)
        {
            throw std::runtime_error("Invalid ledger entry type for RewardEpoch");
        }
    }

    // Ledger entry-specific field getters

    /**
     * @brief Get sfEpochNumber (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getEpochNumber() const
    {
        return this->sle_->at(sfEpochNumber);
    }

    /**
     * @brief Get sfEpochStartLedger (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getEpochStartLedger() const
    {
        return this->sle_->at(sfEpochStartLedger);
    }

    /**
     * @brief Get sfEpochPoolBalance (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_AMOUNT::type::value_type
    getEpochPoolBalance() const
    {
        return this->sle_->at(sfEpochPoolBalance);
    }

    /**
     * @brief Get sfEmissionRate (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_AMOUNT::type::value_type
    getEmissionRate() const
    {
        return this->sle_->at(sfEmissionRate);
    }

    /**
     * @brief Get sfCurrentBurnBps (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT32::type::value_type
    getCurrentBurnBps() const
    {
        return this->sle_->at(sfCurrentBurnBps);
    }

    /**
     * @brief Get sfFeeVolumeEMA (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getFeeVolumeEMA() const
    {
        if (hasFeeVolumeEMA())
            return this->sle_->at(sfFeeVolumeEMA);
        return std::nullopt;
    }

    /**
     * @brief Check if sfFeeVolumeEMA is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasFeeVolumeEMA() const
    {
        return this->sle_->isFieldPresent(sfFeeVolumeEMA);
    }

    /**
     * @brief Get sfAggregateCompositeScore (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getAggregateCompositeScore() const
    {
        if (hasAggregateCompositeScore())
            return this->sle_->at(sfAggregateCompositeScore);
        return std::nullopt;
    }

    /**
     * @brief Check if sfAggregateCompositeScore is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasAggregateCompositeScore() const
    {
        return this->sle_->isFieldPresent(sfAggregateCompositeScore);
    }

    /**
     * @brief Get sfLPAllocationBps (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getLPAllocationBps() const
    {
        if (hasLPAllocationBps())
            return this->sle_->at(sfLPAllocationBps);
        return std::nullopt;
    }

    /**
     * @brief Check if sfLPAllocationBps is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasLPAllocationBps() const
    {
        return this->sle_->isFieldPresent(sfLPAllocationBps);
    }

    /**
     * @brief Get sfAggregateLPShares (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT64::type::value_type>
    getAggregateLPShares() const
    {
        if (hasAggregateLPShares())
            return this->sle_->at(sfAggregateLPShares);
        return std::nullopt;
    }

    /**
     * @brief Check if sfAggregateLPShares is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasAggregateLPShares() const
    {
        return this->sle_->isFieldPresent(sfAggregateLPShares);
    }

    /**
     * @brief Get sfProposals (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_VECTOR256::type::value_type>
    getProposals() const
    {
        if (hasProposals())
            return this->sle_->at(sfProposals);
        return std::nullopt;
    }

    /**
     * @brief Check if sfProposals is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasProposals() const
    {
        return this->sle_->isFieldPresent(sfProposals);
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
 * @brief Builder for RewardEpoch ledger entries.
 *
 * Provides a fluent interface for constructing ledger entries with method chaining.
 * Uses STObject internally for flexible ledger entry construction.
 * Inherits common field setters from LedgerEntryBuilderBase.
 */
class RewardEpochBuilder : public LedgerEntryBuilderBase<RewardEpochBuilder>
{
public:
    /**
     * @brief Construct a new RewardEpochBuilder with required fields.
     * @param epochNumber The sfEpochNumber field value.
     * @param epochStartLedger The sfEpochStartLedger field value.
     * @param epochPoolBalance The sfEpochPoolBalance field value.
     * @param emissionRate The sfEmissionRate field value.
     * @param currentBurnBps The sfCurrentBurnBps field value.
     * @param previousTxnID The sfPreviousTxnID field value.
     * @param previousTxnLgrSeq The sfPreviousTxnLgrSeq field value.
     */
    RewardEpochBuilder(std::decay_t<typename SF_UINT32::type::value_type> const& epochNumber,std::decay_t<typename SF_UINT32::type::value_type> const& epochStartLedger,std::decay_t<typename SF_AMOUNT::type::value_type> const& epochPoolBalance,std::decay_t<typename SF_AMOUNT::type::value_type> const& emissionRate,std::decay_t<typename SF_UINT32::type::value_type> const& currentBurnBps,std::decay_t<typename SF_UINT256::type::value_type> const& previousTxnID,std::decay_t<typename SF_UINT32::type::value_type> const& previousTxnLgrSeq)
        : LedgerEntryBuilderBase<RewardEpochBuilder>(ltREWARD_EPOCH)
    {
        setEpochNumber(epochNumber);
        setEpochStartLedger(epochStartLedger);
        setEpochPoolBalance(epochPoolBalance);
        setEmissionRate(emissionRate);
        setCurrentBurnBps(currentBurnBps);
        setPreviousTxnID(previousTxnID);
        setPreviousTxnLgrSeq(previousTxnLgrSeq);
    }

    /**
     * @brief Construct a RewardEpochBuilder from an existing SLE object.
     * @param sle The existing ledger entry to copy from.
     * @throws std::runtime_error if the ledger entry type doesn't match.
     */
    RewardEpochBuilder(std::shared_ptr<SLE const> sle)
    {
        if (sle->at(sfLedgerEntryType) != ltREWARD_EPOCH)
        {
            throw std::runtime_error("Invalid ledger entry type for RewardEpoch");
        }
        object_ = *sle;
    }

    /** @brief Ledger entry-specific field setters */

    /**
     * @brief Set sfEpochNumber (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setEpochNumber(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfEpochNumber] = value;
        return *this;
    }

    /**
     * @brief Set sfEpochStartLedger (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setEpochStartLedger(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfEpochStartLedger] = value;
        return *this;
    }

    /**
     * @brief Set sfEpochPoolBalance (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setEpochPoolBalance(std::decay_t<typename SF_AMOUNT::type::value_type> const& value)
    {
        object_[sfEpochPoolBalance] = value;
        return *this;
    }

    /**
     * @brief Set sfEmissionRate (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setEmissionRate(std::decay_t<typename SF_AMOUNT::type::value_type> const& value)
    {
        object_[sfEmissionRate] = value;
        return *this;
    }

    /**
     * @brief Set sfCurrentBurnBps (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setCurrentBurnBps(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfCurrentBurnBps] = value;
        return *this;
    }

    /**
     * @brief Set sfFeeVolumeEMA (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setFeeVolumeEMA(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfFeeVolumeEMA] = value;
        return *this;
    }

    /**
     * @brief Set sfAggregateCompositeScore (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setAggregateCompositeScore(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfAggregateCompositeScore] = value;
        return *this;
    }

    /**
     * @brief Set sfLPAllocationBps (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setLPAllocationBps(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfLPAllocationBps] = value;
        return *this;
    }

    /**
     * @brief Set sfAggregateLPShares (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setAggregateLPShares(std::decay_t<typename SF_UINT64::type::value_type> const& value)
    {
        object_[sfAggregateLPShares] = value;
        return *this;
    }

    /**
     * @brief Set sfProposals (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setProposals(std::decay_t<typename SF_VECTOR256::type::value_type> const& value)
    {
        object_[sfProposals] = value;
        return *this;
    }

    /**
     * @brief Set sfPreviousTxnID (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setPreviousTxnID(std::decay_t<typename SF_UINT256::type::value_type> const& value)
    {
        object_[sfPreviousTxnID] = value;
        return *this;
    }

    /**
     * @brief Set sfPreviousTxnLgrSeq (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    RewardEpochBuilder&
    setPreviousTxnLgrSeq(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfPreviousTxnLgrSeq] = value;
        return *this;
    }

    /**
     * @brief Build and return the completed RewardEpoch wrapper.
     * @param index The ledger entry index.
     * @return The constructed ledger entry wrapper.
     */
    RewardEpoch
    build(uint256 const& index)
    {
        return RewardEpoch{std::make_shared<SLE>(std::move(object_), index)};
    }
};

}  // namespace xrpl::ledger_entries
