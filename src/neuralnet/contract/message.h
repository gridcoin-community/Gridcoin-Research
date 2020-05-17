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
}
