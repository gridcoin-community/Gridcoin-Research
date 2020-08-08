#pragma once

#include <string>

class CWalletTx;

namespace NN {

class Contract;

//!
//! \brief Create and send a transaction that contains the provided contract.
//!
//! \param contract A new contract to publish in a transaction.
//!
//! \return Contains the finalized transaction and error message, if any.
//! TODO: refactor to remove string-based signaling.
//!
std::pair<CWalletTx, std::string> SendContract(Contract contract);

//!
//! \brief Send a transaction that contains a contract.
//!
//! This overload enables client code to configure aspects of the transaction
//! that contains contract message.
//!
//! \param wtx Transaction to send that contains a contract.
//!
//! \return Contains the finalized transaction and error message, if any.
//! TODO: refactor to remove string-based signaling.
//!
std::pair<CWalletTx, std::string> SendContract(CWalletTx wtx);
}
