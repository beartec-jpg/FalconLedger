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

class PopLpStateBuilder;

/**
 * @brief Ledger Entry: PopLpState
 *
 * Type: ltPOP_LP_STATE (0x0095)
 * RPC Name: pop_lp_state
 *
 * Immutable wrapper around SLE providing type-safe field access.
 * Use PopLpStateBuilder to construct new ledger entries.
 */
class PopLpState : public LedgerEntryBase
{
public:
    static constexpr LedgerEntryType entryType = ltPOP_LP_STATE;

    /**
     * @brief Construct a PopLpState ledger entry wrapper from an existing SLE object.
     * @throws std::runtime_error if the ledger entry type doesn't match.
     */
    explicit PopLpState(std::shared_ptr<SLE const> sle)
        : LedgerEntryBase(std::move(sle))
    {
        // Verify ledger entry type
        if (sle_->getType() != entryType)
        {
            throw std::runtime_error("Invalid ledger entry type for PopLpState");
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
     * @brief Get sfVaultID (SoeRequired)
     * @return The field value.
     */
    [[nodiscard]]
    SF_UINT256::type::value_type
    getVaultID() const
    {
        return this->sle_->at(sfVaultID);
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
 * @brief Builder for PopLpState ledger entries.
 *
 * Provides a fluent interface for constructing ledger entries with method chaining.
 * Uses STObject internally for flexible ledger entry construction.
 * Inherits common field setters from LedgerEntryBuilderBase.
 */
class PopLpStateBuilder : public LedgerEntryBuilderBase<PopLpStateBuilder>
{
public:
    /**
     * @brief Construct a new PopLpStateBuilder with required fields.
     * @param account The sfAccount field value.
     * @param vaultID The sfVaultID field value.
     * @param previousTxnID The sfPreviousTxnID field value.
     * @param previousTxnLgrSeq The sfPreviousTxnLgrSeq field value.
     */
    PopLpStateBuilder(std::decay_t<typename SF_ACCOUNT::type::value_type> const& account,std::decay_t<typename SF_UINT256::type::value_type> const& vaultID,std::decay_t<typename SF_UINT256::type::value_type> const& previousTxnID,std::decay_t<typename SF_UINT32::type::value_type> const& previousTxnLgrSeq)
        : LedgerEntryBuilderBase<PopLpStateBuilder>(ltPOP_LP_STATE)
    {
        setAccount(account);
        setVaultID(vaultID);
        setPreviousTxnID(previousTxnID);
        setPreviousTxnLgrSeq(previousTxnLgrSeq);
    }

    /**
     * @brief Construct a PopLpStateBuilder from an existing SLE object.
     * @param sle The existing ledger entry to copy from.
     * @throws std::runtime_error if the ledger entry type doesn't match.
     */
    PopLpStateBuilder(std::shared_ptr<SLE const> sle)
    {
        if (sle->at(sfLedgerEntryType) != ltPOP_LP_STATE)
        {
            throw std::runtime_error("Invalid ledger entry type for PopLpState");
        }
        object_ = *sle;
    }

    /** @brief Ledger entry-specific field setters */

    /**
     * @brief Set sfAccount (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    PopLpStateBuilder&
    setAccount(std::decay_t<typename SF_ACCOUNT::type::value_type> const& value)
    {
        object_[sfAccount] = value;
        return *this;
    }

    /**
     * @brief Set sfVaultID (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    PopLpStateBuilder&
    setVaultID(std::decay_t<typename SF_UINT256::type::value_type> const& value)
    {
        object_[sfVaultID] = value;
        return *this;
    }

    /**
     * @brief Set sfLastClaimedEpoch (SoeDefault)
     * @return Reference to this builder for method chaining.
     */
    PopLpStateBuilder&
    setLastClaimedEpoch(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfLastClaimedEpoch] = value;
        return *this;
    }

    /**
     * @brief Set sfPreviousTxnID (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    PopLpStateBuilder&
    setPreviousTxnID(std::decay_t<typename SF_UINT256::type::value_type> const& value)
    {
        object_[sfPreviousTxnID] = value;
        return *this;
    }

    /**
     * @brief Set sfPreviousTxnLgrSeq (SoeRequired)
     * @return Reference to this builder for method chaining.
     */
    PopLpStateBuilder&
    setPreviousTxnLgrSeq(std::decay_t<typename SF_UINT32::type::value_type> const& value)
    {
        object_[sfPreviousTxnLgrSeq] = value;
        return *this;
    }

    /**
     * @brief Build and return the completed PopLpState wrapper.
     * @param index The ledger entry index.
     * @return The constructed ledger entry wrapper.
     */
    PopLpState
    build(uint256 const& index)
    {
        return PopLpState{std::make_shared<SLE>(std::move(object_), index)};
    }
};

}  // namespace xrpl::ledger_entries
