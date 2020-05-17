#pragma once

namespace NN {

class Contract;

//!
//! \brief Stores or processes neural network contract messages.
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
    //! \brief Handle an contract addition.
    //!
    //! \param contract A contract message that describes the addition.
    //!
    virtual void Add(Contract contract) = 0;

    //!
    //! \brief Handle a contract deletion.
    //!
    //! \param contract A contract message that describes the deletion.
    //!
    virtual void Delete(const Contract& contract) = 0;

    //!
    //! \brief Revert a contract found in a disconnected block.
    //!
    //! The application calls this method for each contract in the disconnected
    //! blocks during chain reorganization. The default implementation forwards
    //! the contract object to an appropriate \c Add() or \c Delete() method by
    //! reversing the action specified in the contract.
    //!
    //! \param contract A contract message that describes the action to revert.
    //!
    virtual void Revert(const Contract& contract);
};
}
