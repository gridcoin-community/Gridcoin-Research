// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_HANDSHAKE_H
#define GRIDCOIN_IPC_HANDSHAKE_H

#include "fs.h"
#include "interfaces/init.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ipc {

//! IPC schema/protocol versions embedded in both binaries and compared during
//! the connect handshake (doc/multiprocess_design.md section 4.2). Bump the minor
//! for additive schema changes, the major for breaking ones (which also require a
//! transition release); bump protocol for transport/handshake changes.
//!
//! Major 2: the NodeIdentity model was re-based onto the wallet UUID (former
//! node_id/datadir/genesis_hash retired), a breaking wire change.
//!
//! Major 3: interfaces::Wallet::listCoins was retired with the windowed
//! coin-control model (#3183) and its ordinal removed from wallet.capnp. Cap'n
//! Proto requires consecutive ordinals, so every later Wallet method shifted
//! down by one -- a peer built against major 2 would dispatch each call to its
//! neighbour. Breaking, hence the major. The same release adds the coin channel's
//! own schema (wallet_coin_source.capnp) and Init.makeWalletCoinSource @14.
//! That addition is additive on its own, but it ships INSIDE major 3, so no
//! build carrying major 3 lacks it and the minor restarts at 0 rather than
//! tracking it separately.
//!
//! Minor 1: RowsChangedPayload gained `records` (wallet_tx_source.capnp @4), the
//! changed rows sampled at emission. Additive, so an old node still talks to a new
//! GUI -- but it degrades SILENTLY if the minor is not bumped: the field simply
//! arrives empty, the GUI's "empty means nothing to apply" contract does exactly
//! that, and the Overview list then shows stale rows until the process restarts.
//! The minor is what turns that into the actionable "Update the node" hard fail in
//! ClientHandshake.
constexpr uint32_t IPC_SCHEMA_MAJOR = 3;
constexpr uint32_t IPC_SCHEMA_MINOR = 0;
constexpr uint32_t IPC_PROTOCOL_VERSION = 1;

//! Domain-separation tag hashed into the node identity token. Bump the suffix if
//! the token *algorithm* ever changes (independent of the capnp schema version).
constexpr char IDENTITY_TOKEN_DOMAIN[] = "gridcoin-node-identity-v1";

//! This process's build fingerprint (git commit, build time, schema/protocol
//! versions) for the handshake build-info exchange.
interfaces::BuildInfo GetLocalBuildInfo();

//! Constant-time byte comparison of two strings (equal length required for a
//! match). Used for the cookie compare so a timing side-channel can't leak it.
bool ConstantTimeEqual(const std::string& a, const std::string& b);

//! Node side: write a fresh 256-bit cookie to <dir>/ipc.cookie (0600, atomic
//! rename) and return its hex. Called once per node startup.
std::string WriteCookie(const fs::path& dir);

//! GUI side: read the hex cookie from <dir>/ipc.cookie. Returns nullopt when the
//! file is absent (the node is not running -- do not dial).
std::optional<std::string> ReadCookie(const fs::path& dir);

//! Compute the node identity token: HEX(SHA256(domain-tag ‖ LE32(len) ‖ uuid)).
//! Deterministic in \p wallet_uuid; returns "" (identity unavailable) when the
//! UUID is empty. The raw UUID never crosses the wire -- only this hash does.
std::string ComputeIdentityToken(const std::vector<unsigned char>& wallet_uuid);

//! Soft (non-fatal) handshake findings surfaced to the GUI.
enum class SoftWarn {
    GitCommitMismatch, //!< GUI and node built from different commits (banner).
    GuiOlderMinor,     //!< GUI schema_minor < node: forward-compatible (log-only).
};

//! Result of the connect handshake. On ok==false the connection must be rejected
//! and \p error shown/logged. On ok==true \p remote_build / \p remote_ident are
//! valid and \p soft carries any non-fatal findings.
struct HandshakeResult {
    bool ok = false;
    std::string error;
    interfaces::BuildInfo remote_build;
    interfaces::NodeIdentity remote_ident;
    std::vector<SoftWarn> soft;
};

//! GUI side: the authentication + compatibility half of the connect handshake
//! (design section 4.3). Against a freshly-connected remote Init, calls
//! authenticate(cookie), getBuildInfo() and getIdentity() -- all inside one
//! try/catch, so an IPC throw (daemon vanished mid-handshake) becomes a clean
//! ok==false rather than an escaping exception. Hard-fail (ok==false) on: wrong
//! cookie, schema_major mismatch, protocol_version mismatch, GUI schema_minor >
//! node's, and a network mismatch vs \p local_network. Soft findings (git_commit
//! mismatch, GUI older minor) are returned in \p soft, not failures. The identity
//! token is returned in \p remote_ident for the caller's GUI-side binding step
//! (CheckIdentityBinding) -- this function does not judge it.
//! \p local defaults to this build's GetLocalBuildInfo(); it is a parameter only
//! so tests can drive every comparison branch (e.g. GUI-newer-minor) regardless
//! of the compiled-in schema constants.
HandshakeResult ClientHandshake(interfaces::Init& init, const std::string& cookie,
                                const std::string& local_network,
                                const interfaces::BuildInfo& local = GetLocalBuildInfo());

//! Outcome of comparing a node's reported identity token against the GUI's stored
//! expectation for this datadir.
enum class BindOutcome {
    FirstSeen,         //!< No stored token: persist \p reported and proceed.
    Match,             //!< Stored == reported: proceed.
    Mismatch,          //!< Stored != reported: prompt / -autotrustidentity / exit.
    UnavailableFresh,  //!< Reported empty, nothing stored: proceed unbound (log once).
    UnavailableStored, //!< Reported empty but a token was stored: treat as a mismatch
                       //!< (a downgrade signal -- never silently skip).
};

//! Pure comparison of a reported token against a stored one. No I/O; the caller
//! owns the QSettings read/write and the prompt. See BindOutcome.
BindOutcome CheckIdentityBinding(const std::string& reported_token, const std::string& stored_token);

} // namespace ipc

#endif // GRIDCOIN_IPC_HANDSHAKE_H
