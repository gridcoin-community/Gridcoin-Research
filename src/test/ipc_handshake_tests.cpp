// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "tinyformat.h"
#include "interfaces/init.h"
#include "ipc/handshake.h"
#include "util/time.h"

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

BOOST_AUTO_TEST_CASE(cookie_round_trip_and_delete)
{
    // Own temp directory rather than GetDataDir(): this only needs somewhere to put a
    // file, and the datadir path applies an owner-only DACL on first creation.
    const fs::path dir = fs::temp_directory_path() / strprintf("grc_ipc_cookie_%d", GetTimeMillis());
    fs::create_directories(dir);

    // Seeded directly rather than through WriteCookie(). WriteCookie applies an
    // owner-only DACL to the temp file it renames into place and THROWS where the
    // platform cannot -- which is the case under Wine, where the Windows
    // cross-compile leg runs this suite ("could not be restricted to the current
    // user -- its DACL is not protected"). The delete path is what this case is
    // about, and it has to be testable on every platform the suite runs on.
    const std::string cookie(64, 'a');
    {
        fsbridge::ofstream out(dir / "ipc.cookie", std::ios::binary);
        out << cookie;
    }

    const std::optional<std::string> read_back = ipc::ReadCookie(dir);
    BOOST_REQUIRE(read_back.has_value());
    BOOST_CHECK_EQUAL(*read_back, cookie);

    ipc::DeleteCookie(dir);

    // Gone from disk, and gone as far as a GUI is concerned -- ReadCookie returning
    // nullopt is what "no node is running" means to ipc::connect.
    BOOST_CHECK(!fs::exists(dir / "ipc.cookie"));
    BOOST_CHECK(!ipc::ReadCookie(dir).has_value());

    // Removing an absent cookie is not an error. Shutdown must stay quiet after an
    // unclean previous exit, or when a user removed the file by hand.
    BOOST_CHECK_NO_THROW(ipc::DeleteCookie(dir));

    // And the real writer where the platform allows it, so the pair is covered
    // end to end on the platforms that can express the permission.
    try {
        const std::string written = ipc::WriteCookie(dir);
        BOOST_CHECK_EQUAL(written.size(), 64u); // 256 bits, hex

        const std::optional<std::string> written_back = ipc::ReadCookie(dir);
        BOOST_REQUIRE(written_back.has_value());
        BOOST_CHECK_EQUAL(*written_back, written);

        ipc::DeleteCookie(dir);
        BOOST_CHECK(!ipc::ReadCookie(dir).has_value());
    } catch (const std::runtime_error& e) {
        BOOST_TEST_MESSAGE(std::string("WriteCookie is not available here: ") + e.what());
    }

    fs::remove_all(dir);
}

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
