#pragma once

#include <string>

class CWalletTx;

namespace NN { class Contract; }

//!
//! \brief Create and send a transaction that contains the provided contract.
//!
//! \param contract A new contract to publish in a transaction.
//!
//! \return Contains the finalized transaction and error message, if any.
//! TODO: refactor to remove string-based signaling.
//!
std::pair<CWalletTx, std::string> SendContract(NN::Contract contract);

//!
//! \brief Create and send a transaction that contains the provided public,
//! non-administrative contract.
//!
//! \param contract A new contract to publish in a transaction.
//!
//! \return An empty string upon success or a description of the error.
//! TODO: refactor to remove string-based signaling.
//!
std::string SendPublicContract(NN::Contract contract);
