// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "tinyformat.h"
#include "interfaces/init.h"
#include "ipc/handshake.h"
#include "test/wine.h"
#include "util/time.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {
//! Minimal interfaces::Init stub: drives ClientHandshake with canned responses so
//! every matching-policy branch is exercised deterministically, no socket. Only
//! the four handshake methods are overridden; the base Init defaults (return
//! nullptr) cover the makeX factories.
struct FakeInit : public interfaces::Init {
    bool auth_ok{true};
    bool throw_on_identity{false};
    bool slot_held{true};
    interfaces::BuildInfo build;
    interfaces::NodeIdentity ident;

    bool authenticate(const std::string&) override { return auth_ok; }
    interfaces::BuildInfo getBuildInfo() override { return build; }
    interfaces::NodeIdentity getIdentity() override
    {
        if (throw_on_identity) throw std::runtime_error("daemon vanished mid-handshake");
        return ident;
    }
    bool holdsAttachSlot() override { return slot_held; }
};

//! What ClientHandshake reports when another GUI holds the core's attach slot.
const std::string ALREADY_ATTACHED_MESSAGE{"Another Gridcoin GUI is already attached to this core. Only one GUI can "
                                           "attach at a time; close the other one first."};

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
    // nullopt is what stops ipc::ConnectToNode before it dials.
    BOOST_CHECK(!fs::exists(dir / "ipc.cookie"));
    BOOST_CHECK(!ipc::ReadCookie(dir).has_value());

    // Removing an absent cookie is not an error. Shutdown must stay quiet after an
    // unclean previous exit, or when a user removed the file by hand.
    BOOST_CHECK_NO_THROW(ipc::DeleteCookie(dir));

    // And the real writer, so the pair is covered end to end. Only Wine is excused:
    // it keeps no protected DACL, so the owner-only cookie cannot be written there.
    // Anywhere else a failure here is the test's.
    try {
        const std::string written = ipc::WriteCookie(dir);
        BOOST_CHECK_EQUAL(written.size(), 64u); // 256 bits, hex

        const std::optional<std::string> written_back = ipc::ReadCookie(dir);
        BOOST_REQUIRE(written_back.has_value());
        BOOST_CHECK_EQUAL(*written_back, written);

        ipc::DeleteCookie(dir);
        BOOST_CHECK(!ipc::ReadCookie(dir).has_value());
    } catch (const std::runtime_error& e) {
        if (!grc_test::RunningUnderWine()) {
            boost::system::error_code ec;
            fs::remove_all(dir, ec);
            throw;
        }
        BOOST_TEST_MESSAGE(std::string("WriteCookie is not available under Wine: ") + e.what());
    }

    fs::remove_all(dir);
}

#ifndef WIN32
// With a group the cookie is 0640 in that group, and without one 0600, both
// readable back through ReadCookie.
BOOST_AUTO_TEST_CASE(cookie_with_group_is_0640)
{
    const fs::path dir = fs::temp_directory_path() / strprintf("grc_ipc_cookie_grp_%d", GetTimeMillis());
    fs::create_directories(dir);
    const fs::path file = dir / "ipc.cookie";
    const auto mode_of = [&] {
        struct stat st;
        BOOST_REQUIRE_EQUAL(::lstat(file.c_str(), &st), 0);
        return static_cast<mode_t>(st.st_mode & 07777);
    };
    const auto group_of = [&] {
        struct stat st;
        BOOST_REQUIRE_EQUAL(::lstat(file.c_str(), &st), 0);
        return st.st_gid;
    };

    const std::string with_group = ipc::WriteCookie(dir, ::getegid());
    BOOST_CHECK_EQUAL(mode_of(), static_cast<mode_t>(0640));
    BOOST_CHECK_EQUAL(group_of(), ::getegid());
    BOOST_CHECK_EQUAL(ipc::ReadCookie(dir).value_or(""), with_group);
    BOOST_CHECK(!fs::exists(dir / "ipc.cookie.tmp"));

    // A group other than the effective one shows the fchown is what sets it.
    const int count = ::getgroups(0, nullptr);
    std::vector<gid_t> groups(count > 0 ? static_cast<size_t>(count) : 0);
    const int n = groups.empty() ? 0 : ::getgroups(count, groups.data());
    for (int i = 0; i < n; ++i) {
        if (groups[i] == ::getegid()) continue;
        ipc::WriteCookie(dir, groups[i]);
        BOOST_CHECK_EQUAL(mode_of(), static_cast<mode_t>(0640));
        BOOST_CHECK_EQUAL(group_of(), groups[i]);
        break;
    }

    const std::string without_group = ipc::WriteCookie(dir);
    BOOST_CHECK_EQUAL(mode_of(), static_cast<mode_t>(0600));
    BOOST_CHECK_EQUAL(ipc::ReadCookie(dir).value_or(""), without_group);
    BOOST_CHECK(with_group != without_group);

    fs::remove_all(dir);
}
#endif

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
    BOOST_CHECK(!r.already_attached);
}

// ---- ClientHandshake (attach slot) ----

// A compatible core that another GUI is attached to: the handshake fails and
// says so, in the words the GUI shows.
BOOST_AUTO_TEST_CASE(handshake_reports_already_attached)
{
    const auto local = MakeLocal();
    FakeInit fake;
    fake.build = local;
    fake.ident.network = "main";
    fake.slot_held = false;
    const auto r = ipc::ClientHandshake(fake, "c", "main", local);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(r.already_attached);
    BOOST_CHECK(r.error.find("already attached") != std::string::npos);
    BOOST_CHECK_EQUAL(r.error, ALREADY_ATTACHED_MESSAGE);
}

// The slot is asked for only after the compatibility checks: an older core
// does not serve holdsAttachSlot, so a mismatch must be reported as itself.
BOOST_AUTO_TEST_CASE(handshake_schema_error_precedes_slot_check)
{
    const auto local = MakeLocal();
    FakeInit fake;
    fake.build = local;
    fake.ident.network = "main";
    fake.slot_held = false;
    fake.build.schema_major += 1;
    const auto r = ipc::ClientHandshake(fake, "c", "main", local);
    BOOST_CHECK(!r.ok);
    BOOST_CHECK(!r.already_attached);
    BOOST_CHECK(r.error.find("schema major") != std::string::npos);
}

// ---- Connect retry policy ----

// Only a core that may still be starting (no cookie yet, or nothing listening)
// is waited for, and only for IPC_CONNECT_WAIT_MAX.
BOOST_AUTO_TEST_CASE(retry_policy)
{
    using K = ipc::ConnectFailureKind;
    using S = ipc::ConnectStep;

    BOOST_CHECK(ipc::IPC_CLIENT_HANDSHAKE_TIMEOUT == std::chrono::seconds{30});
    BOOST_CHECK(ipc::IPC_CONNECT_RETRY_INTERVAL == std::chrono::seconds{2});
    BOOST_CHECK(ipc::IPC_CONNECT_WAIT_MAX == std::chrono::seconds{300});

    BOOST_CHECK(ipc::NextConnectStep({K::NoCookie, ""}, std::chrono::seconds{10}) == S::Retry);
    BOOST_CHECK(ipc::NextConnectStep({K::NoCookie, ""}, std::chrono::seconds{299}) == S::Retry);
    BOOST_CHECK(ipc::NextConnectStep({K::NoCookie, ""}, std::chrono::seconds{300}) == S::GiveUp);
    BOOST_CHECK(ipc::NextConnectStep({K::NotListening, ""}, std::chrono::seconds{0}) == S::Retry);
    BOOST_CHECK(ipc::NextConnectStep({K::NotListening, ""}, std::chrono::seconds{299}) == S::Retry);
    BOOST_CHECK(ipc::NextConnectStep({K::NotListening, ""}, std::chrono::seconds{300}) == S::GiveUp);
    for (auto k : {K::PeerRefused, K::AlreadyAttached, K::Timeout, K::Handshake}) {
        BOOST_CHECK(ipc::NextConnectStep({k, ""}, std::chrono::seconds{0}) == S::GiveUp);
    }

    // A default failure is a handshake failure, which is never retried.
    BOOST_CHECK(ipc::ConnectFailure{}.kind == K::Handshake);
}

BOOST_AUTO_TEST_SUITE_END()
