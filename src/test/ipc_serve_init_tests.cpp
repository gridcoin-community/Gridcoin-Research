// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/init.h"
#include "ipc/handshake.h"
#include "ipc/serve_init.h"

#include <boost/test/unit_test.hpp>

#include <memory>
#include <stdexcept>
#include <string>

namespace {
//! Inner Init the served wrapper delegates to. isCoreReady() is overridden so we
//! can observe that it is reachable only after authentication; getIdentity() would
//! be ignored (ServeInit serves the identity it was constructed with), so it is
//! left as the base default.
struct StubInit : public interfaces::Init {
    bool isCoreReady() override { return true; }
};
} // namespace

BOOST_AUTO_TEST_SUITE(ipc_serve_init_tests)

// The serve-side counterpart to ipc_handshake_tests' client coverage: ServeInit
// must refuse to serve build/identity/delegated methods until authenticate()
// succeeds with the right cookie, and must serve THIS process's build info + the
// identity it was handed (not the inner Init's).
BOOST_AUTO_TEST_CASE(serve_init_gates_everything_on_authentication)
{
    interfaces::NodeIdentity id;
    id.identity_token = "expected-token";
    id.network = "main";
    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "s3cr3t-cookie", id);

    // Pre-auth: nothing is served, and the connection reports itself unauthenticated
    // (which is what the listener's deadline consults).
    BOOST_CHECK(!serve->isAuthenticated());
    BOOST_CHECK_THROW(serve->getBuildInfo(), std::exception);
    BOOST_CHECK_THROW(serve->getIdentity(), std::exception);
    BOOST_CHECK_THROW(serve->isCoreReady(), std::exception);

    // Correct cookie: authenticate() succeeds and the gate opens.
    BOOST_CHECK(serve->authenticate("s3cr3t-cookie"));
    BOOST_CHECK(serve->isAuthenticated());
    BOOST_CHECK_NO_THROW(serve->getBuildInfo());
    BOOST_CHECK(serve->isCoreReady());

    // getIdentity() serves the identity ServeInit was constructed with.
    const interfaces::NodeIdentity served = serve->getIdentity();
    BOOST_CHECK_EQUAL(served.identity_token, "expected-token");
    BOOST_CHECK_EQUAL(served.network, "main");
}

// One strike: a wrong cookie latches the connection permanently unauthenticated,
// so it cannot be used as a guessing oracle and cannot go on holding the node's
// single connection slot in the hope of getting it right. The listener's
// authentication deadline then reclaims the slot. A legitimate GUI presents one
// cookie and gives up on rejection, so it never sees this.
BOOST_AUTO_TEST_CASE(serve_init_wrong_cookie_latches_the_connection)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "s3cr3t-cookie", id);

    BOOST_CHECK(!serve->authenticate("wrong-cookie"));
    BOOST_CHECK(!serve->isAuthenticated());
    BOOST_CHECK_THROW(serve->getBuildInfo(), std::exception);

    // The RIGHT cookie is now refused too: retrying on this connection is not a
    // path back in. This is the assertion that distinguishes a latch from merely
    // "authenticate() returned false".
    BOOST_CHECK(!serve->authenticate("s3cr3t-cookie"));
    BOOST_CHECK(!serve->isAuthenticated());
    BOOST_CHECK_THROW(serve->getBuildInfo(), std::exception);
    BOOST_CHECK_THROW(serve->isCoreReady(), std::exception);

    // Recovery is by reconnecting, which builds a fresh ServeInit.
    auto reconnected = ipc::MakeServeInit(std::make_unique<StubInit>(), "s3cr3t-cookie", id);
    BOOST_CHECK(reconnected->authenticate("s3cr3t-cookie"));
    BOOST_CHECK(reconnected->isAuthenticated());
}

// An empty expected cookie must authenticate nobody. ConstantTimeEqual("", "") is
// true, so without the explicit guard an empty cookie would open the gate to every
// peer -- and it must not latch either, since nothing the peer did was wrong.
BOOST_AUTO_TEST_CASE(serve_init_empty_cookie_authenticates_nobody)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "", id);

    BOOST_CHECK(!serve->authenticate(""));
    BOOST_CHECK(!serve->authenticate("anything"));
    BOOST_CHECK(!serve->isAuthenticated());
    BOOST_CHECK_THROW(serve->getBuildInfo(), std::exception);
}

BOOST_AUTO_TEST_CASE(serve_init_getbuildinfo_is_this_builds_info)
{
    interfaces::NodeIdentity id;
    id.network = "main";
    auto serve = ipc::MakeServeInit(std::make_unique<StubInit>(), "cookie", id);
    BOOST_CHECK(serve->authenticate("cookie"));
    // Served build info is this process's embedded fingerprint, not the inner's.
    BOOST_CHECK_EQUAL(serve->getBuildInfo().git_commit, ipc::GetLocalBuildInfo().git_commit);
    BOOST_CHECK_EQUAL(serve->getBuildInfo().schema_major, ipc::IPC_SCHEMA_MAJOR);
}

// Per-connection auth (Phase 3): the node builds a FRESH ServeInit for each
// accepted connection (make_serve_init factory), so authentication is per
// instance. A second connection -- a distinct ServeInit -- starts gated even
// after an earlier one authenticated; auth can never leak across connections.
BOOST_AUTO_TEST_CASE(serve_init_authentication_is_per_instance)
{
    interfaces::NodeIdentity id;
    id.network = "main";

    // First "connection": authenticate it.
    auto first = ipc::MakeServeInit(std::make_unique<StubInit>(), "cookie", id);
    BOOST_CHECK(first->authenticate("cookie"));
    BOOST_CHECK(first->isCoreReady());

    // Second "connection" (a fresh ServeInit) is independently gated: it must
    // present the cookie itself; the first instance's auth does not carry over.
    auto second = ipc::MakeServeInit(std::make_unique<StubInit>(), "cookie", id);
    BOOST_CHECK_THROW(second->getBuildInfo(), std::exception);
    BOOST_CHECK_THROW(second->isCoreReady(), std::exception);
    BOOST_CHECK(second->authenticate("cookie"));
    BOOST_CHECK(second->isCoreReady());
}

BOOST_AUTO_TEST_SUITE_END()
