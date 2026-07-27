// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "interfaces/init.h"
#include "ipc/handshake.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
//! Minimal interfaces::Init stub: drives ClientHandshake with canned responses so
//! every matching-policy branch is exercised deterministically, no socket. Only
//! the three handshake methods are overridden; the base Init defaults (return
//! nullptr) cover the makeX factories.
struct FakeInit : public interfaces::Init {
    bool auth_ok{true};
    bool throw_on_identity{false};
    interfaces::BuildInfo build;
    interfaces::NodeIdentity ident;

    bool authenticate(const std::string&) override { return auth_ok; }
    interfaces::BuildInfo getBuildInfo() override { return build; }
    interfaces::NodeIdentity getIdentity() override
    {
        if (throw_on_identity) throw std::runtime_error("daemon vanished mid-handshake");
        return ident;
    }
};

//! A local build the fakes below match for a clean handshake. schema_minor is 1 so
//! both the GUI-newer (hard) and GUI-older (soft) minor branches are reachable.
interfaces::BuildInfo MakeLocal()
{
    interfaces::BuildInfo b;
    b.git_commit = "local-commit";
    b.built_at = "now";
    b.schema_major = 2;
    b.schema_minor = 1;
    b.protocol_version = 1;
    return b;
}

bool HasSoft(const ipc::HandshakeResult& r, ipc::SoftWarn w)
{
    return std::find(r.soft.begin(), r.soft.end(), w) != r.soft.end();
}
} // namespace

BOOST_AUTO_TEST_SUITE(ipc_handshake_tests)

// ---- ComputeIdentityToken ----

BOOST_AUTO_TEST_CASE(identity_token_empty_uuid_is_empty)
{
    BOOST_CHECK(ipc::ComputeIdentityToken({}).empty());
}

BOOST_AUTO_TEST_CASE(identity_token_is_deterministic_and_hex_sha256)
{
    const std::vector<unsigned char> uuid{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    BOOST_CHECK_EQUAL(ipc::ComputeIdentityToken(uuid), ipc::ComputeIdentityToken(uuid));
    BOOST_CHECK_EQUAL(ipc::ComputeIdentityToken(uuid).size(), 64u); // 32-byte SHA256 as hex
}

BOOST_AUTO_TEST_CASE(identity_token_changes_with_uuid)
{
    const std::vector<unsigned char> a{1, 2, 3}, b{1, 2, 4};
    BOOST_CHECK(ipc::ComputeIdentityToken(a) != ipc::ComputeIdentityToken(b));
    // Different lengths must also differ (the LE32 length prefix guarantees it).
    const std::vector<unsigned char> c{1, 2};
    BOOST_CHECK(ipc::ComputeIdentityToken(a) != ipc::ComputeIdentityToken(c));
}

// ---- CheckIdentityBinding ----

BOOST_AUTO_TEST_CASE(bind_first_seen)
{
    BOOST_CHECK(ipc::CheckIdentityBinding("tok", "") == ipc::BindOutcome::FirstSeen);
}

BOOST_AUTO_TEST_CASE(bind_match)
{
    BOOST_CHECK(ipc::CheckIdentityBinding("tok", "tok") == ipc::BindOutcome::Match);
}

BOOST_AUTO_TEST_CASE(bind_mismatch)
{
    BOOST_CHECK(ipc::CheckIdentityBinding("tok", "other") == ipc::BindOutcome::Mismatch);
}

BOOST_AUTO_TEST_CASE(bind_unavailable_fresh)
{
    BOOST_CHECK(ipc::CheckIdentityBinding("", "") == ipc::BindOutcome::UnavailableFresh);
}

BOOST_AUTO_TEST_CASE(bind_unavailable_stored_is_a_downgrade)
{
    // Node reports empty but the GUI had a bound token: must be a downgrade signal,
    // never a silent skip (an attacker could force the node to report empty).
    BOOST_CHECK(ipc::CheckIdentityBinding("", "tok") == ipc::BindOutcome::UnavailableStored);
}

// ---- ClientHandshake (hard fails) ----

BOOST_AUTO_TEST_CASE(handshake_rejects_bad_cookie)
{
    FakeInit f;
    f.auth_ok = false;
    const auto r = ipc::ClientHandshake(f, "cookie", "main", MakeLocal());
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!r.error.empty());
}

BOOST_AUTO_TEST_CASE(handshake_rejects_schema_major_mismatch)
{
    const auto local = MakeLocal();
    FakeInit f;
    f.build = local;
    f.build.schema_major += 1;
    f.ident.network = "main";
    const auto r = ipc::ClientHandshake(f, "c", "main", local);
    BOOST_CHECK(!r.ok);
}

BOOST_AUTO_TEST_CASE(handshake_rejects_protocol_mismatch)
{
    const auto local = MakeLocal();
    FakeInit f;
    f.build = local;
    f.build.protocol_version += 1;
    f.ident.network = "main";
    const auto r = ipc::ClientHandshake(f, "c", "main", local);
    BOOST_CHECK(!r.ok);
}

BOOST_AUTO_TEST_CASE(handshake_rejects_gui_newer_minor)
{
    const auto local = MakeLocal(); // schema_minor 1
    FakeInit f;
    f.build = local;
    f.build.schema_minor = 0; // node is older -> GUI newer -> hard fail
    f.ident.network = "main";
    const auto r = ipc::ClientHandshake(f, "c", "main", local);
    BOOST_CHECK(!r.ok);
}

BOOST_AUTO_TEST_CASE(handshake_rejects_network_mismatch)
{
    const auto local = MakeLocal();
    FakeInit f;
    f.build = local;
    f.ident.network = "test"; // GUI asked for main
    const auto r = ipc::ClientHandshake(f, "c", "main", local);
    BOOST_CHECK(!r.ok);
}

BOOST_AUTO_TEST_CASE(handshake_ipc_throw_is_clean_failure)
{
    const auto local = MakeLocal();
    FakeInit f;
    f.build = local;
    f.ident.network = "main";
    f.throw_on_identity = true; // getIdentity() throws mid-handshake
    const auto r = ipc::ClientHandshake(f, "c", "main", local);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!r.error.empty());
    BOOST_CHECK(r.soft.empty());
}

// ---- ClientHandshake (soft findings + clean) ----

BOOST_AUTO_TEST_CASE(handshake_soft_gui_older_minor)
{
    const auto local = MakeLocal(); // schema_minor 1
    FakeInit f;
    f.build = local;
    f.build.schema_minor = 2; // node newer -> forward-compatible soft finding (log-only)
    f.ident.network = "main";
    const auto r = ipc::ClientHandshake(f, "c", "main", local);
    BOOST_CHECK(r.ok);
    BOOST_CHECK(HasSoft(r, ipc::SoftWarn::GuiOlderMinor));
}

BOOST_AUTO_TEST_CASE(handshake_soft_git_commit_mismatch)
{
    const auto local = MakeLocal();
    FakeInit f;
    f.build = local;
    f.build.git_commit = "a-different-commit";
    f.ident.network = "main";
    const auto r = ipc::ClientHandshake(f, "c", "main", local);
    BOOST_CHECK(r.ok);
    BOOST_CHECK(HasSoft(r, ipc::SoftWarn::GitCommitMismatch));
}

BOOST_AUTO_TEST_CASE(handshake_clean_match_no_soft)
{
    const auto local = MakeLocal();
    FakeInit f;
    f.build = local;
    f.ident.network = "main";
    f.ident.identity_token = "deadbeef";
    const auto r = ipc::ClientHandshake(f, "c", "main", local);
    BOOST_CHECK(r.ok);
    BOOST_CHECK(r.soft.empty());
    BOOST_CHECK_EQUAL(r.remote_ident.identity_token, "deadbeef");
    BOOST_CHECK_EQUAL(r.remote_build.git_commit, local.git_commit);
}

BOOST_AUTO_TEST_SUITE_END()
