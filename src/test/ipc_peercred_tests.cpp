// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/peercred.h"

#include <boost/test/unit_test.hpp>

#include <cstring>

#ifndef WIN32
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

BOOST_AUTO_TEST_SUITE(ipc_peercred_tests)

// The enforcement description is logged at daemon startup, so it must at least
// exist and be non-empty on every platform.
BOOST_AUTO_TEST_CASE(peercred_enforcement_is_described)
{
    const char* mode = ipc::PeerCredentialEnforcement();
    BOOST_REQUIRE(mode != nullptr);
    BOOST_CHECK(std::strlen(mode) > 0);
}

#ifndef WIN32

// The rejection branch. This is the assertion the old suite could not make: the
// previous tests (an unavailable fd is allowed, a socketpair is allowed) both
// passed verbatim against `return true;`, so they could not distinguish the
// implementation from a stub. PeerUidAllowed is the comparison factored out of
// CheckPeerCredentials precisely so a foreign uid can be exercised without
// needing a second OS user.
BOOST_AUTO_TEST_CASE(peercred_foreign_uid_is_rejected)
{
    const uid_t self = ::geteuid();

    BOOST_CHECK(ipc::PeerUidAllowed(self, self));

    // A different uid is refused whichever way it differs.
    BOOST_CHECK(!ipc::PeerUidAllowed(self + 1, self));
    BOOST_CHECK(!ipc::PeerUidAllowed(self, self + 1));

    // Root is not special-cased in either direction: uid 0 talking to a non-root
    // server is a mismatch, and a root server does not accept arbitrary users.
    if (self != 0) {
        BOOST_CHECK(!ipc::PeerUidAllowed(0, self));
        BOOST_CHECK(!ipc::PeerUidAllowed(self, 0));
    }
}

// Fail-closed contract: a peer fd that cannot be inspected is REFUSED. This
// reverses the previous behaviour (which allowed and logged), so it is asserted
// explicitly -- a regression here silently turns the uid check into a no-op.
BOOST_AUTO_TEST_CASE(peercred_unavailable_fd_is_refused)
{
    BOOST_CHECK(!ipc::CheckPeerCredentials(-1));
}

// An fd that is open but is not a socket cannot answer SO_PEERCRED / getpeereid.
// That is a genuine error path, and it must refuse rather than fall through.
BOOST_AUTO_TEST_CASE(peercred_non_socket_fd_is_refused)
{
    int fds[2];
    BOOST_REQUIRE_EQUAL(::pipe(fds), 0);
    BOOST_CHECK(!ipc::CheckPeerCredentials(fds[0]));
    ::close(fds[0]);
    ::close(fds[1]);
}

// Both ends of a socketpair belong to THIS process, so the peer uid equals our
// own effective uid and CheckPeerCredentials must accept both ends. This covers
// the end-to-end happy path (the getsockopt actually succeeds and reports us);
// the rejection half is covered by PeerUidAllowed above.
BOOST_AUTO_TEST_CASE(peercred_same_user_socketpair_is_allowed)
{
    int sv[2];
    BOOST_REQUIRE_EQUAL(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    BOOST_CHECK(ipc::CheckPeerCredentials(sv[0]));
    BOOST_CHECK(ipc::CheckPeerCredentials(sv[1]));
    ::close(sv[0]);
    ::close(sv[1]);
}

#endif // !WIN32

BOOST_AUTO_TEST_SUITE_END()
