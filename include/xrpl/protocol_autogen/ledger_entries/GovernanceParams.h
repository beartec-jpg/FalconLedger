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

class GovernanceParamsBuilder;

/**
 * @brief Ledger Entry: GovernanceParams
 *
 * Type: ltGOVERNANCE_PARAMS (0x0093)
 * RPC Name: governance_params
 *
 * Immutable wrapper around SLE providing type-safe field access.
 * Use GovernanceParamsBuilder to construct new ledger entries.
 */
class GovernanceParams : public LedgerEntryBase
{
public:
    static constexpr LedgerEntryType entryType = ltGOVERNANCE_PARAMS;

    /**
     * @brief Construct a GovernanceParams ledger entry wrapper from an existing SLE object.
     * @throws std::runtime_error if the ledger entry type doesn't match.
     */
    explicit GovernanceParams(std::shared_ptr<SLE const> sle)
        : LedgerEntryBase(std::move(sle))
    {
        // Verify ledger entry type
        if (sle_->getType() != entryType)
        {
            throw std::runtime_error("Invalid ledger entry type for GovernanceParams");
        }
    }

    // Ledger entry-specific field getters

    /**
     * @brief Get sfCurrentBurnBps (SoeDefault)
     * @return The field value, or std::nullopt if not present.
     */
    [[nodiscard]]
    protocol_autogen::Optional<SF_UINT32::type::value_type>
    getCurrentBurnBps() const
    {
        if (hasCurrentBurnBps())
            return this->sle_->at(sfCurrentBurnBps);
        return std::nullopt;
    }

    /**
     * @brief Check if sfCurrentBurnBps is present.
     * @return True if the field is present, false otherwise.
     */
    [[nodiscard]]
    bool
    hasCurrentBurnBps() const
    {
        return this->sle_->isFieldPresent(sfCurrentBurnBps);
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
 * @brief Builder for GovernanceParams ledger entries.
 *
 * Provides a fluent interface for constructing ledger entries with method chaining.
 * Uses STObject internally for flexible ledger entry construction.
 * Inherits common field setters from LedgerEntryBuilderBase.
 */
class GovernanceParamsBuilder : public LedgerEntryBuilderBase<GovernanceParamsBuilder>
{
public:
    /**
     * @brief Construct a new GovernanceParamsBuilder with required fields.
     * @param previousTxnID The sfPreviousTxnID field value.
     * @param previousTxnLgrSeq The sfPreviousTxnLgrSeq field value.
     */
    GovernanceParamsBuilder(std::decay_t<typename SF_UINT256::type::value_type> const& previousTxnID,std::decay_t<typename SF_UINT32::type::value_type> const& previousTxnLgrSeq)
        : LedgerEntryBuilderBase<GovernanceParamsBuilder>(ltGOVERNANCE_PARAMS)
    {
        setPreviousTxnID(previousTxnID);
        setPreviousTxnLgrSeq(previousTxnLgrSeq);
    }

    /**
     * @brief Construct a GovernanceParamsBuilder from an existing SLE object.
     * @param sle The existing ledger entry to copy from.
     * @throws std::runtime_error if the ledger entry type doesn't match.
     */
    GovernanceParamsBuilder(std::shared_ptr<SLE const> sle)
    {
        if (sle->at(sfLedgerEntryType) != ltGOVERNANCE_PARAMS)
        {
            throw std::runtime_error("Invalid ledger entry type for GovernanceParams");
        }
        object_ = *sle;
    }

    /** @brief Ledger entry-specific field setters */

    /**
     * @brief Set sfCurrentBurnBps (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    GovernanceParamsBuilder&
    setCurrentBurnBps(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfCurrentBurnBps] = value;
        return *this;
    }

    /**
     * @brief Set sfPreviousTxnID (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceParamsBuilder&
    setPreviousTxnID(std::decay_t<typename SF_UINT256::type::value_type> const& value)
    {
        object_[sfPreviousTxnID] = value;
        return *this;
    }

    /**
     * @brief Set sfPreviousTxnLgrSeq (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    GovernanceParamsBuilder&
    setPreviousTxnLgrSeq(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfPreviousTxnLgrSeq] = value;
        return *this;
    }

    /**
     * @brief Build and return the completed GovernanceParams wrapper.
     * @param index The ledger entry index.
     * @return The constructed ledger entry wrapper.
     */
    GovernanceParams
    build(uint256 const& index)
    {
        return GovernanceParams{std::make_shared<SLE>(std::move(object_), index)};
    }
};

}  // namespace xrpl::ledger_entries
