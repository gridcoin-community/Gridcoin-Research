// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_WALLET_CAPABILITY_H
#define GRIDCOIN_WALLET_CAPABILITY_H

#include <atomic>
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
    AutoUnlockSetup, // creating or replacing an autounlock secret (the Windows and macOS integrations)
    RpcRequest,      // one RPC request carrying the elevation header
};

//! Names one capability issued by CWallet::BeginElevation(). 0 is never issued.
using CapabilityId = uint64_t;

//! \brief Whether \p kind is one of the enumerators above.
//!
//! A kind that arrives from another process crosses as an integer, and every
//! integer of the underlying type is a value of the enumeration. This tells the
//! kinds this build knows from the rest, which are refused.
bool IsKnownCapabilityKind(CapabilityKind kind);

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

//! \brief Lets the caller give up the passphrase attempt it makes on this thread
//! while the attempt waits.
//!
//! Passphrase attempts take turns (CWallet::Unlock(), BeginElevation() and
//! ChangeWalletPassphrase()). While this is in force on a thread and \p cancel
//! is true, an attempt that thread makes stops waiting: one still waiting for
//! its turn returns without its passphrase being checked, and one that was
//! refused stops waiting out its delay. A check already under way finishes.
//!
//! It is seen only by the thread that sets it. An attempt made through IPC, as
//! the GUI's are in the split build, runs on a thread of the node, where the
//! client's cancel cannot reach it: the served wallet sets a token there that
//! the closing of the client's connection sets, and otherwise only a shutdown
//! of the node ends its wait. RPC sets none.
//!
//! Neither copyable nor movable, and not allocated with new: it is in force
//! for the block that holds it, on the thread that runs that block.
class PassphraseAttemptCancel
{
public:
    explicit PassphraseAttemptCancel(const std::atomic<bool>& cancel);
    ~PassphraseAttemptCancel();

    PassphraseAttemptCancel(const PassphraseAttemptCancel&) = delete;
    PassphraseAttemptCancel& operator=(const PassphraseAttemptCancel&) = delete;
    PassphraseAttemptCancel(PassphraseAttemptCancel&&) = delete;
    PassphraseAttemptCancel& operator=(PassphraseAttemptCancel&&) = delete;

    static void* operator new(std::size_t) = delete;
    static void* operator new[](std::size_t) = delete;

    //! Whether the attempt on the calling thread has been cancelled. False on a
    //! thread that set none.
    static bool Requested();

private:
    //! What was in force on this thread before, restored when this ends.
    const std::atomic<bool>* m_previous{nullptr};
};

//! \brief Ends a capability however the block that began it is left, a throw
//! included, so that it is not left to its deadline.
//!
//! The capability is ended with CWallet::EndElevation(), which takes neither
//! cs_main nor cs_wallet; neither copyable nor movable, it lives in the block
//! of the operation the capability was begun for.
class EndElevationOnExit
{
public:
    EndElevationOnExit(CWallet& wallet, CapabilityId id) : m_wallet(wallet), m_id(id) {}
    ~EndElevationOnExit();

    EndElevationOnExit(const EndElevationOnExit&) = delete;
    EndElevationOnExit& operator=(const EndElevationOnExit&) = delete;
    EndElevationOnExit(EndElevationOnExit&&) = delete;
    EndElevationOnExit& operator=(EndElevationOnExit&&) = delete;

private:
    CWallet& m_wallet;
    CapabilityId m_id{0};
};

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
//! guards and ends there, on the same thread. A scope that ends on another
//! thread all the same, as one inside an object on the heap can, fails closed:
//! its grant stops counting on the thread that opened it (see ElevationGrant).
//! A holder that serves calls on more than one thread opens a scope for each
//! call, on the thread that serves it.
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
