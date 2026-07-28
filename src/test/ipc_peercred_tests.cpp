// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/peercred.h"

#include <boost/test/unit_test.hpp>

#ifndef WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

BOOST_AUTO_TEST_SUITE(ipc_peercred_tests)

// A peer fd that is not available (< 0) cannot be inspected. CheckPeerCredentials
// does not enforce what it cannot read -- the cookie remains the root of trust --
// so it allows and logs rather than rejecting a connection it simply could not
// examine. (On Windows there is no peer-credential API at all, and this is the
// only meaningful case.)
BOOST_AUTO_TEST_CASE(peercred_unavailable_fd_is_allowed)
{
    BOOST_CHECK(ipc::CheckPeerCredentials(-1));
}

#ifndef WIN32
// Both ends of a socketpair belong to THIS process, so the peer uid equals our
// own effective uid and CheckPeerCredentials must accept both ends. A cross-user
// rejection cannot be exercised from a single-process unit test; it is validated
// live (the mesh) and by the same-user requirement documented for split runs.
BOOST_AUTO_TEST_CASE(peercred_same_user_socketpair_is_allowed)
{
    int sv[2];
    BOOST_REQUIRE_EQUAL(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    BOOST_CHECK(ipc::CheckPeerCredentials(sv[0]));
    BOOST_CHECK(ipc::CheckPeerCredentials(sv[1]));
    ::close(sv[0]);
    ::close(sv[1]);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
