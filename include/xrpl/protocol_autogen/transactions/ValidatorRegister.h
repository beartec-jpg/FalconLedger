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

class ValidatorRegisterBuilder;

/**
 * @brief Transaction: ValidatorRegister
 *
 * Type: ttVALIDATOR_REGISTER (85)
 * Delegable: Delegation::NotDelegable
 * Amendment: featureProofOfParticipation
 * Privileges: NoPriv
 *
 * Immutable wrapper around STTx providing type-safe field access.
 * Use ValidatorRegisterBuilder to construct new transactions.
 */
class ValidatorRegister : public TransactionBase
{
public:
    static constexpr xrpl::TxType txType = ttVALIDATOR_REGISTER;

    /**
     * @brief Construct a ValidatorRegister transaction wrapper from an existing STTx object.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    explicit ValidatorRegister(std::shared_ptr<STTx const> tx)
        : TransactionBase(std::move(tx))
    {
        // Verify transaction type
        if (tx_->getTxnType() != txType)
        {
            throw std::runtime_error("Invalid transaction type for ValidatorRegister");
        }
    }

    // Transaction-specific field getters
};

/**
 * @brief Builder for ValidatorRegister transactions.
 *
 * Provides a fluent interface for constructing transactions with method chaining.
 * Uses STObject internally for flexible transaction construction.
 * Inherits common field setters from TransactionBuilderBase.
 */
class ValidatorRegisterBuilder : public TransactionBuilderBase<ValidatorRegisterBuilder>
{
public:
    /**
     * @brief Construct a new ValidatorRegisterBuilder with required fields.
     * @param account The account initiating the transaction.
     * @param sequence Optional sequence number for the transaction.
     * @param fee Optional fee for the transaction.
     */
    ValidatorRegisterBuilder(SF_ACCOUNT::type::value_type account,
                    std::optional<SF_UINT32::type::value_type> sequence = std::nullopt,
                    std::optional<SF_AMOUNT::type::value_type> fee = std::nullopt
)
        : TransactionBuilderBase<ValidatorRegisterBuilder>(ttVALIDATOR_REGISTER, account, sequence, fee)
    {
    }

    /**
     * @brief Construct a ValidatorRegisterBuilder from an existing STTx object.
     * @param tx The existing transaction to copy from.
     * @throws std::runtime_error if the transaction type doesn't match.
     */
    ValidatorRegisterBuilder(std::shared_ptr<STTx const> tx)
    {
        if (tx->getTxnType() != ttVALIDATOR_REGISTER)
        {
            throw std::runtime_error("Invalid transaction type for ValidatorRegisterBuilder");
        }
        object_ = *tx;
    }

    /** @brief Transaction-specific field setters */

    /**
     * @brief Build and return the ValidatorRegister wrapper.
     * @param publicKey The public key for signing.
     * @param secretKey The secret key for signing.
     * @return The constructed transaction wrapper.
     */
    ValidatorRegister
    build(PublicKey const& publicKey, SecretKey const& secretKey)
    {
        sign(publicKey, secretKey);
        return ValidatorRegister{std::make_shared<STTx>(std::move(object_))};
    }
};

}  // namespace xrpl::transactions
