// Copyright (c) 2014-2020 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

class CBlockIndex;
class CTransaction;

namespace GRC {

class Contract;

//!
//! \brief Refers to a contract and its blockchain context.
//!
//! Contract handlers perform actions in response to contract messages in the
//! transactions in blocks added to or removed from the blockchain. This type
//! holds references to the objects that provide context for the locations of
//! the contracts passed to a contract handler.
//!
class ContractContext
{
public:
    const Contract& m_contract;        //!< Contract to process.
    const CTransaction& m_tx;          //!< Transaction with the contract.
    const CBlockIndex* const m_pindex; //!< Block index for the contract height.

    // Not needed yet--transactions only allow one contract each right now:
    //const size_t m_offset;             //!< Contract offset in the transaction.

    //!
    //! \brief Initialize a contract context.
    //!
    //! \param contract Contract to process.
    //! \param tx       Transaction that contains the contract.
    //! \param pindex   Block index for the contract height.
    //!
    ContractContext(
        const Contract& contract,
        const CTransaction& tx,
        const CBlockIndex* const pindex)
        : m_contract(contract)
        , m_tx(tx)
        , m_pindex(pindex)
    {
    }

    const Contract& operator*() const noexcept { return m_contract; }
    const Contract* operator->() const noexcept { return &m_contract; }
};

//!
//! \brief Stores or processes Gridcoin contract messages.
//!
//! Typically, contract handler implementations only mutate the data they store
//! when the application processes contract messages embedded in a transaction.
//! Consumers of the data will usually access it in a read-only fashion because
//! the data is based on immutable values stored in connected blocks.
//!
//! For this reason, contract handlers NEED NOT to implement the interface in a
//! thread-safe manner. The application applies contract messages from only one
//! thread. However, a contract handler MUST guarantee thread-safety for any of
//! its methods that provide the read-only access to the contract data imported
//! by this interface.
//!
struct IContractHandler
{
    //!
    //! \brief Destructor.
    //!
    virtual ~IContractHandler() {}

    //!
    //! \brief Perform contextual validation for the provided contract.
    //!
    //! \param contract Contract to validate.
    //! \param tx       Transaction that contains the contract.
    //!
    //! \return \c false If the contract fails validation.
    //!
    virtual bool Validate(const Contract& contract, const CTransaction& tx) const = 0;

    //!
    //! \brief Destroy the contract handler state to prepare for historical
    //! contract replay.
    //!
    virtual void Reset() = 0;

    //!
    //! \brief Handle an contract addition.
    //!
    //! \param ctx References the contract and associated context.
    //!
    virtual void Add(const ContractContext& ctx) = 0;

    //!
    //! \brief Handle a contract deletion.
    //!
    //! \param ctx References the contract and associated context.
    //!
    virtual void Delete(const ContractContext& ctx) = 0;

    //!
    //! \brief Revert a contract found in a disconnected block.
    //!
    //! The application calls this method for each contract in the disconnected
    //! blocks during chain reorganization. The default implementation forwards
    //! the contract object to an appropriate \c Add() or \c Delete() method by
    //! reversing the action specified in the contract.
    //!
    //! \param ctx References the contract and associated context.
    //!
    virtual void Revert(const ContractContext& ctx);
};
}
