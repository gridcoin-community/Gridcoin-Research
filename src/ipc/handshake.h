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

namespace ipc {

//! IPC schema/protocol versions embedded in both binaries and compared during
//! the connect handshake (doc/multiprocess_design.md section 4.2). Bump the minor
//! for additive schema changes, the major for breaking ones (which also require a
//! transition release); bump protocol for transport/handshake changes.
constexpr uint32_t IPC_SCHEMA_MAJOR = 1;
constexpr uint32_t IPC_SCHEMA_MINOR = 0;
constexpr uint32_t IPC_PROTOCOL_VERSION = 1;

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

//! Node side: load the persistent per-datadir node_id from
//! <dir>/node_identity.json (generating a new UUID on first run), then rewrite
//! the file with the current network / datadir / genesis and return the identity.
interfaces::NodeIdentity WriteIdentity(const fs::path& dir);

//! GUI side: read <dir>/node_identity.json. Returns nullopt when absent or
//! malformed.
std::optional<interfaces::NodeIdentity> ReadIdentity(const fs::path& dir);

//! GUI side: run the authentication + build-info compatibility half of the
//! connect handshake (design section 4.3 steps 5-6) against a freshly-connected
//! remote Init. Calls authenticate(\p cookie) then getBuildInfo() and compares
//! versions against this build. Returns true on success; on failure returns false
//! and sets \p error_out to a human-readable reason. Hard-fail conditions: wrong
//! cookie, schema_major mismatch (incompatible wire format), protocol_version
//! mismatch, and the GUI's schema_minor being newer than the node's. (The
//! identity binding and the git_commit soft-warn are handled GUI-side.)
bool ClientAuthenticateAndCheck(interfaces::Init& init, const std::string& cookie, std::string& error_out);

} // namespace ipc

#endif // GRIDCOIN_IPC_HANDSHAKE_H
