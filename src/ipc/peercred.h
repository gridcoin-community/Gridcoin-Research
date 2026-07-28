// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_PEERCRED_H
#define GRIDCOIN_IPC_PEERCRED_H

namespace ipc {

//! Verify that the peer on an accepted AF_UNIX connection is the SAME OS user as
//! this (serving) process. This is defense-in-depth on top of the cookie: the
//! cookie file is already owner-only (0600 in an owner-only datadir), so a
//! different user should never possess it -- but rejecting a foreign uid up front
//! closes the connection before any handshake, rather than relying solely on the
//! cookie gate.
//!
//! Platform coverage: Linux uses SO_PEERCRED; *BSD/macOS use getpeereid(); on
//! Windows AF_UNIX exposes no peer-credential API, so this returns true and the
//! owner-only datadir NTFS ACL on node.sock + ipc.cookie remains the guard (see
//! ipc/process.cpp).
//!
//! Returns false ONLY on a definite uid mismatch (logged). If the peer uid cannot
//! be determined (peer_fd < 0, or the getsockopt/getpeereid call fails), it logs
//! and returns true -- we do not hard-fail a connection we simply could not
//! inspect, because the cookie remains the root of trust.
bool CheckPeerCredentials(int peer_fd);

} // namespace ipc

#endif // GRIDCOIN_IPC_PEERCRED_H
