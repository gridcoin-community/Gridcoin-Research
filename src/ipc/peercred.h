// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_PEERCRED_H
#define GRIDCOIN_IPC_PEERCRED_H

#ifndef WIN32
#include <sys/types.h> // uid_t
#endif

namespace ipc {

#ifndef WIN32
//! The uid comparison, factored out of CheckPeerCredentials so both branches are
//! unit-testable (the rejection branch cannot be reached from a single-process
//! test, because both ends of a socketpair necessarily share our uid).
//!
//! Same-uid only. Root is NOT special-cased: a root peer is a different uid and
//! is refused. A root process can bypass this at will (it can ptrace us or read
//! the cookie), so admitting it would buy nothing and would widen the rule from
//! "one user's processes" to "one user's processes, plus root" for no gain.
bool PeerUidAllowed(uid_t peer_uid, uid_t self_uid);
#endif

//! Verify that the peer on an accepted AF_UNIX connection is the SAME OS user as
//! this process. This is defense-in-depth on top of the cookie: the cookie file is
//! already owner-only (0600 in an owner-only datadir), so a different user should
//! never possess it -- but rejecting a foreign uid up front closes the connection
//! before any handshake, rather than relying solely on the cookie gate.
//!
//! Usable from BOTH ends of the socket. The node calls it on each accepted
//! connection; the GUI calls it on its own connected fd before presenting the
//! cookie, so a FOREIGN-uid impostor that won the race to bind node.sock is
//! refused before it can harvest the bearer token. A SAME-uid impostor is not
//! excluded by this and cannot be -- nothing at the OS level distinguishes two
//! processes of the same user.
//!
//! FAILS CLOSED on POSIX (changed 2026-08: it previously returned true on every
//! error path). A peer we cannot inspect is refused, because "could not inspect"
//! and "inspected and it matched" are not the same statement, and the difference
//! is exactly what an attacker would arrange. In this codebase peer_fd always
//! comes from an accepted or connected AF_UNIX socket, so the error paths are
//! not reachable in normal operation; if one ever fires, the log line says which
//! call failed and MP refuses to serve rather than silently degrading to
//! cookie-only. Only a definite uid mismatch is logged via error().
//!
//! Platform coverage: Linux uses SO_PEERCRED with struct ucred; every other POSIX
//! target we support uses getpeereid(). Note OpenBSD ALSO defines SO_PEERCRED, but
//! its payload is struct sockpeercred -- so the implementation gates on __linux__
//! rather than on the option name, which would not compile on OpenBSD. On
//! Windows AF_UNIX exposes no peer-credential API whatsoever, so this returns
//! true and the explicit owner+SYSTEM PROTECTED DACL that ipc/process.cpp applies
//! to node.sock and ipc.cookie (and verifies by read-back) is the guard. Call
//! PeerCredentialEnforcement() to log which of these is in force.
//! How to treat a peer whose uid cannot be determined.
enum class PeerCredPolicy {
    //! Refuse unless the uid is positively confirmed to match. For the NODE's
    //! accept path, where this is a security boundary and peer_fd always comes
    //! from a socket we just accepted.
    Enforcing,
    //! Refuse only on a DEFINITE mismatch; allow (with a log line) when the uid
    //! cannot be read.
    //!
    //! CURRENTLY UNUSED, and deliberately so. This was the GUI's connect-side
    //! policy, justified as defense-in-depth "backstopped by the cookie". That
    //! justification does not survive contact with the call order: the client's
    //! next act after this check is to hand the cookie to whatever answered, so the
    //! cookie cannot backstop a check that gates its own disclosure. The connect
    //! side is now Enforcing.
    //!
    //! Retained because the role distinction it encodes is real -- a check that is
    //! genuinely supplementary, gating nothing that is disclosed on failure, could
    //! legitimately use it. Do not reach for it to work around a platform that
    //! cannot answer; that turns "we could not verify" into "we proceeded anyway".
    //!
    //! MEASURED (macOS 14.8.7, xnu-10063, 2026-08-10): Darwin DOES populate peer
    //! credentials on both ends -- a bind/listen/connect probe read the correct uid
    //! from the connecting fd as well as the accepted one. An earlier revision of
    //! this comment speculated that it might not, and used that as the reason for
    //! Advisory; that speculation was wrong and is recorded here so nobody
    //! rediscovers it. Advisory is kept on the role argument above, not on a claim
    //! that macOS cannot answer -- it can. Client-side support on other BSDs and on
    //! future kj backends remains untested, which is what the policy still buys.
    Advisory,
};

bool CheckPeerCredentials(int peer_fd, PeerCredPolicy policy = PeerCredPolicy::Enforcing);

//! One-line description of the peer-credential enforcement actually compiled in,
//! for a startup log line. An operator reading debug.log should never have to
//! guess whether the uid check is real on this platform.
const char* PeerCredentialEnforcement();

} // namespace ipc

#endif // GRIDCOIN_IPC_PEERCRED_H
