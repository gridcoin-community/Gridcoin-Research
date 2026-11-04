// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_CAPABILITY_H
#define GRIDCOIN_WALLET_CAPABILITY_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

class CWallet;
class ElevationGrant;

//! What an elevation authorises. One operation kind per capability.
enum class CapabilityKind : uint8_t {
    Send,            // send flows (create, fee prompt, commit), incl. sendmany-like GUI flows
    SignMessage,
    SignPSGT,        // signPSGT, signPoolEntry
    Contract,        // manual beacon advertise, votes, polls, MRC
    Backup,          // wallet snapshot for the transfer channel
    FileTransfer,    // dumpwallet / importwallet streams, console file commands
    Settings,        // Elevate-class settings: sidestakes, proxy, upnp, settxfee
    AutoUnlockSetup, // creating or replacing an autounlock secret (P4, P5)
    RpcRequest,      // one RPC request carrying the elevation header
};

//! Names one capability issued by CWallet::BeginElevation(). 0 is never issued.
using CapabilityId = uint64_t;

//! \brief How long a capability of \p kind lasts from the moment it is issued.
//!
//! Send, SignPSGT, Contract, Backup and RpcRequest 300 s; SignMessage, Settings
//! and AutoUnlockSetup 120 s; FileTransfer 3600 s.
std::chrono::seconds CapabilityTtl(CapabilityKind kind);

//! \brief How long a failed passphrase attempt waits before it returns.
//!
//! \p consecutive_failures counts this failure and the ones before it since the
//! last success, so the first failure waits 500 ms. The delay is
//! min(250 ms << min(n, 7), 30 s): it doubles with each further failure and is
//! 30 s from the seventh on.
std::chrono::milliseconds FailedPassphraseDelay(unsigned int consecutive_failures);

//! \brief Lets the calling thread use the wallet's keys for one operation, under
//! a capability issued for that kind of operation.
//!
//! The constructor looks the capability up. It is active when \p id names one
//! that is still issued (EndElevation() has not ended it), was issued for
//! \p kind, and has not passed its deadline. An active scope holds an
//! ElevationGrant for the wallet, which CWallet::CanUseKeys() and the keystore's
//! general key access then accept on this thread, while the wallet-wide unlock
//! scope stays what it was. An inactive scope changes nothing; the operation it
//! guards is then refused as it would be without one.
//!
//! The check is made once, when the scope is opened. A capability that ends or
//! expires while a scope is open does not close it: the operation in progress
//! finishes, and every scope opened afterwards is inactive. The scope never ends
//! the capability itself; EndElevation() does.
//!
//! The grant begins and ends on the thread that constructs the scope, and is
//! seen by no other. The scope is therefore neither copyable nor movable, and it
//! cannot be allocated with new: it lives in the block of the operation it
//! guards and ends there, on the same thread.
class CapabilityScope
{
public:
    CapabilityScope(CWallet& wallet, std::optional<CapabilityId> id, CapabilityKind kind);
    ~CapabilityScope();

    CapabilityScope(const CapabilityScope&) = delete;
    CapabilityScope& operator=(const CapabilityScope&) = delete;
    CapabilityScope(CapabilityScope&&) = delete;
    CapabilityScope& operator=(CapabilityScope&&) = delete;

    static void* operator new(std::size_t) = delete;
    static void* operator new[](std::size_t) = delete;

    //! Whether the capability was valid when the scope was opened, so that the
    //! scope holds a grant.
    bool Active() const { return m_grant != nullptr; }

private:
    //! The grant, held for the scope's life; null when the scope is inactive.
    std::unique_ptr<ElevationGrant> m_grant;
};

#endif // GRIDCOIN_WALLET_CAPABILITY_H
