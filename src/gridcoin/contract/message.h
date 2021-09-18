// Copyright (c) 2014-2021 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_CONTRACT_MESSAGE_H
#define GRIDCOIN_CONTRACT_MESSAGE_H

#include <string>

class CWalletTx;

namespace GRC {

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

#endif // GRIDCOIN_CONTRACT_MESSAGE_H
