// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_INIT_H
#define GRIDCOIN_INTERFACES_INIT_H

#include <memory>

namespace interfaces {

class Node;
class Wallet;

//! Per-process bootstrap interface: hands out the other interfaces. In the
//! monolithic build MakeGridcoinInit() returns an implementation whose
//! factories construct the in-process wrappers; in the Stage 2 multiprocess
//! build the GUI process receives an Init proxy whose factories return IPC
//! proxies instead (after the authentication and matching handshake described
//! in doc/multiprocess_design.md).
//!
//! The default implementations return nullptr so each process type overrides
//! only what it supports.
class Init
{
public:
    virtual ~Init() = default;

    virtual std::unique_ptr<Node> makeNode() { return nullptr; }

    //! Returns the interface for the node's single wallet. May return nullptr
    //! before wallet startup completes.
    virtual std::unique_ptr<Wallet> makeWallet() { return nullptr; }
};

//! Return the monolithic-build Init implementation.
std::unique_ptr<Init> MakeGridcoinInit();

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_INIT_H
