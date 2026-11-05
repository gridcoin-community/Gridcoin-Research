// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "ipc/win_attest.h"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#ifdef WIN32
#include "test/wine.h"
#include "tinyformat.h"

#include <thread>
#include <utility>

#include <windows.h>
#endif

using namespace ipc::win;
using namespace util::winacl;

namespace {

// The rights a peer has on the pipe, by value: FILE_READ_DATA 0x1, FILE_WRITE_DATA
// 0x2, FILE_READ_ATTRIBUTES 0x80, READ_CONTROL 0x20000 and SYNCHRONIZE 0x100000.
constexpr uint32_t PEER_RIGHTS = 0x1 | 0x2 | 0x80 | 0x20000 | 0x100000;

// FILE_APPEND_DATA, which on a pipe is FILE_CREATE_PIPE_INSTANCE.
constexpr uint32_t CREATE_PIPE_INSTANCE = 0x4;

// FILE_ALL_ACCESS.
constexpr uint32_t ALL_ACCESS = 0x1F01FF;

#ifdef WIN32
static_assert(PEER_RIGHTS == (FILE_READ_DATA | FILE_WRITE_DATA | FILE_READ_ATTRIBUTES | READ_CONTROL | SYNCHRONIZE));
static_assert(CREATE_PIPE_INSTANCE == FILE_CREATE_PIPE_INSTANCE && CREATE_PIPE_INSTANCE == FILE_APPEND_DATA);
static_assert((FILE_GENERIC_WRITE & CREATE_PIPE_INSTANCE) != 0);
static_assert(ALL_ACCESS == FILE_ALL_ACCESS);
static_assert(INTEGRITY_MEDIUM_RID == SECURITY_MANDATORY_MEDIUM_RID);
static_assert(IMPERSONATION_IDENTIFICATION == SecurityIdentification);
#endif

Sid SidOf(const std::string& text)
{
    const std::optional<Sid> sid = ParseSidString(text);
    BOOST_REQUIRE_MESSAGE(sid.has_value(), "'" << text << "' does not parse");
    return *sid;
}

//! A local account, standing in for the server's own account.
Sid Self()
{
    return SidOf("S-1-5-21-1004336348-1177238915-682003330-1001");
}

//! Another local account on the same computer.
Sid OtherUser()
{
    return SidOf("S-1-5-21-1004336348-1177238915-682003330-1002");
}

//! A local group standing in for the attach group.
Sid Group()
{
    return SidOf("S-1-5-21-1004336348-1177238915-682003330-1005");
}

//! The facts of an ordinary desktop process of \p user's: Medium integrity, at the
//! identification level, not an AppContainer.
TokenFacts Facts(const Sid& user)
{
    TokenFacts facts;
    facts.user = user;
    facts.integrity_rid = 0x2000;
    facts.impersonation_level = 1;
    return facts;
}

void CheckRefused(const Classification& c, const std::string& word)
{
    BOOST_CHECK(c.cls == PeerClass::Refused);
    BOOST_CHECK_MESSAGE(c.reason.find(word) != std::string::npos, "'" << c.reason << "' does not name " << word);
}

} // namespace

BOOST_AUTO_TEST_SUITE(ipc_win_attest_tests)

// The peer's rights on the pipe are exactly the five named ones, and never the right
// to create an instance of the pipe's name.
BOOST_AUTO_TEST_CASE(attest_peer_mask_names_each_right)
{
    BOOST_CHECK_EQUAL(ATTEST_PEER_MASK, PEER_RIGHTS);
    BOOST_CHECK_EQUAL(ATTEST_PEER_MASK & CREATE_PIPE_INSTANCE, 0U);
}

// The pipe is owned by the server's account, which has full access, and the attach
// group is given the peer's rights only when configured. Nothing is inheritable.
BOOST_AUTO_TEST_CASE(attest_pipe_acl_spec)
{
    const AclSpec same_account = AttestPipeAclSpec(Self(), std::nullopt);
    BOOST_CHECK(same_account.owner == Self());
    BOOST_REQUIRE_EQUAL(same_account.aces.size(), 1U);
    BOOST_CHECK(same_account.aces[0].trustee == Self());
    BOOST_CHECK_EQUAL(same_account.aces[0].mask, ALL_ACCESS);
    BOOST_CHECK(same_account.aces[0].inherit == Inherit::None);

    const AclSpec with_group = AttestPipeAclSpec(Self(), Group());
    BOOST_CHECK(with_group.owner == Self());
    BOOST_REQUIRE_EQUAL(with_group.aces.size(), 2U);
    BOOST_CHECK(with_group.aces[0].trustee == Self());
    BOOST_CHECK_EQUAL(with_group.aces[0].mask, ALL_ACCESS);
    BOOST_CHECK(with_group.aces[1].trustee == Group());
    BOOST_CHECK_EQUAL(with_group.aces[1].mask, PEER_RIGHTS);
    BOOST_CHECK(with_group.aces[1].inherit == Inherit::None);
}

BOOST_AUTO_TEST_CASE(classify_owner)
{
    const Classification c = ClassifyTokenFacts(Facts(Self()), Self(), false);
    BOOST_CHECK(c.cls == PeerClass::Owner);

    // The owner is the owner whether or not an attach group is configured, and
    // whether or not it is a member.
    TokenFacts member = Facts(Self());
    member.member_of_attach_group = true;
    BOOST_CHECK(ClassifyTokenFacts(member, Self(), true).cls == PeerClass::Owner);
}

BOOST_AUTO_TEST_CASE(classify_attach_group_member)
{
    TokenFacts facts = Facts(OtherUser());
    facts.member_of_attach_group = true;
    BOOST_CHECK(ClassifyTokenFacts(facts, Self(), true).cls == PeerClass::AttachGroup);

    // Without a configured group, membership counts for nothing.
    BOOST_CHECK(ClassifyTokenFacts(facts, Self(), false).cls == PeerClass::Refused);
    BOOST_CHECK(!ClassifyTokenFacts(facts, Self(), false).reason.empty());

    // Another account that is not a member is refused.
    facts.member_of_attach_group = false;
    const Classification c = ClassifyTokenFacts(facts, Self(), true);
    BOOST_CHECK(c.cls == PeerClass::Refused);
    BOOST_CHECK_MESSAGE(c.reason.find(FormatSid(OtherUser())) != std::string::npos, c.reason);
}

// An elevated GUI, even of the server's own account, is refused with a reason that
// says so (High 0x3000, System 0x4000).
BOOST_AUTO_TEST_CASE(classify_high_integrity_reason_names_elevation)
{
    TokenFacts facts = Facts(Self());
    facts.integrity_rid = 0x3000;
    CheckRefused(ClassifyTokenFacts(facts, Self(), false), "elevated");

    facts.integrity_rid = 0x4000;
    CheckRefused(ClassifyTokenFacts(facts, Self(), false), "elevated");

    // Medium plus is above Medium too.
    facts.integrity_rid = 0x2100;
    CheckRefused(ClassifyTokenFacts(facts, Self(), false), "elevated");

    // An elevated member of the attach group is refused as well.
    TokenFacts member = Facts(OtherUser());
    member.member_of_attach_group = true;
    member.integrity_rid = 0x3000;
    CheckRefused(ClassifyTokenFacts(member, Self(), true), "elevated");
}

BOOST_AUTO_TEST_CASE(classify_anonymous_level_refused)
{
    TokenFacts facts = Facts(Self());
    facts.impersonation_level = 0;
    CheckRefused(ClassifyTokenFacts(facts, Self(), false), "anonymous");
}

BOOST_AUTO_TEST_CASE(classify_appcontainer_refused)
{
    TokenFacts facts = Facts(Self());
    facts.is_appcontainer = true;
    CheckRefused(ClassifyTokenFacts(facts, Self(), false), "AppContainer");

    TokenFacts member = Facts(OtherUser());
    member.member_of_attach_group = true;
    member.is_appcontainer = true;
    CheckRefused(ClassifyTokenFacts(member, Self(), true), "AppContainer");
}

BOOST_AUTO_TEST_CASE(classify_anonymous_logon_refused)
{
    TokenFacts facts = Facts(wellknown::AnonymousLogon());
    facts.member_of_attach_group = true;
    CheckRefused(ClassifyTokenFacts(facts, Self(), true), "ANONYMOUS LOGON");

    // Even when the server's own account is given as the anonymous one.
    CheckRefused(ClassifyTokenFacts(facts, wellknown::AnonymousLogon(), true), "ANONYMOUS LOGON");
}

// A user that is not a SID is never anyone's: not the owner's, even when the server's
// own SID is just as malformed.
BOOST_AUTO_TEST_CASE(classify_malformed_user_refused)
{
    TokenFacts facts = Facts(Sid{});
    BOOST_CHECK(ClassifyTokenFacts(facts, Sid{}, false).cls == PeerClass::Refused);

    facts.member_of_attach_group = true;
    BOOST_CHECK(ClassifyTokenFacts(facts, Self(), true).cls == PeerClass::Refused);
}

BOOST_AUTO_TEST_CASE(pipe_suffix_and_nonce_validation)
{
    const std::string suffix = "0123456789abcdef0123456789abcdef";
    const std::string nonce = suffix + "fedcba9876543210fedcba9876543210";
    BOOST_CHECK(IsValidPipeSuffix(suffix));
    BOOST_CHECK(IsValidPipeSuffix(std::string(32, 'a')));
    BOOST_CHECK(IsValidNonceHex(nonce));
    BOOST_CHECK(IsValidNonceHex(std::string(64, '0')));

    // Each is only the other's length apart.
    BOOST_CHECK(!IsValidPipeSuffix(nonce));
    BOOST_CHECK(!IsValidNonceHex(suffix));

    const std::vector<std::string> bad_suffixes{
        "",
        "0123456789ABCDEF0123456789abcdef",  // upper case
        suffix.substr(1),                    // 31
        suffix + "0",                        // 33
        "..\\x",
        "\\\\host",
        "..\\x" + suffix.substr(4),          // 32 characters
        "\\\\host\\pipe\\" + suffix.substr(12),
        "0123456789abcdeg0123456789abcdef",  // not hex
        std::string("0123456789abcdef0123456789abcde\0", 32),
        "0123456789abcdef0123456789abcd\xc3\xa9",
    };
    for (const std::string& bad : bad_suffixes) {
        BOOST_CHECK_MESSAGE(!IsValidPipeSuffix(bad), "suffix '" << bad << "' accepted");
    }

    const std::vector<std::string> bad_nonces{
        "",
        "0123456789ABCDEF" + nonce.substr(16), // upper case
        nonce.substr(1),                       // 63
        nonce + "0",                           // 65
        "..\\x" + nonce.substr(4),
        "\\\\host" + nonce.substr(6),
        nonce.substr(0, 63) + "g",
        nonce.substr(0, 63) + " ",
    };
    for (const std::string& bad : bad_nonces) {
        BOOST_CHECK_MESSAGE(!IsValidNonceHex(bad), "nonce '" << bad << "' accepted");
    }
}

// The path is the prefix and a valid suffix, and nothing at all for any other suffix.
BOOST_AUTO_TEST_CASE(pipe_path_only_from_a_valid_suffix)
{
    const std::string suffix = "0123456789abcdef0123456789abcdef";
    const std::optional<std::wstring> path = AttestPipePath(suffix);
    BOOST_REQUIRE(path.has_value());
    BOOST_CHECK(*path == L"\\\\.\\pipe\\gridcoin-attest-0123456789abcdef0123456789abcdef");
    BOOST_CHECK(*path == std::wstring(ATTEST_PIPE_PREFIX) + std::wstring(suffix.begin(), suffix.end()));

    BOOST_CHECK(!AttestPipePath(""));
    BOOST_CHECK(!AttestPipePath("..\\x" + suffix.substr(4)));
    BOOST_CHECK(!AttestPipePath("\\\\host\\pipe\\" + suffix.substr(12)));
    BOOST_CHECK(!AttestPipePath("0123456789ABCDEF0123456789abcdef"));
}

// Finish is in time up to the deadline after Begin and refused after it, and it waits
// about a second at most for the nonce, far less than the deadline.
BOOST_AUTO_TEST_CASE(attest_finish_in_time_only_until_the_deadline)
{
    using std::chrono::milliseconds;
    const std::chrono::steady_clock::time_point begun = std::chrono::steady_clock::time_point{} + std::chrono::hours{1};

    BOOST_CHECK(FinishInTime(begun, begun, ATTEST_DEADLINE));
    BOOST_CHECK(FinishInTime(begun, begun + ATTEST_DEADLINE, ATTEST_DEADLINE));
    BOOST_CHECK(!FinishInTime(begun, begun + ATTEST_DEADLINE + milliseconds{1}, ATTEST_DEADLINE));
    BOOST_CHECK(FinishInTime(begun, begun + milliseconds{100}, milliseconds{100}));
    BOOST_CHECK(!FinishInTime(begun, begun + milliseconds{150}, milliseconds{100}));

    BOOST_CHECK(ATTEST_FINISH_WAIT <= std::chrono::seconds{1});
    BOOST_CHECK(ATTEST_FINISH_WAIT < ATTEST_DEADLINE);
}

#ifdef WIN32
namespace {

//! Whether these cases can rely on Windows' own security model. Wine impersonates the
//! process's own token and keeps no real access control, so under it they only log.
bool SkipUnderWine()
{
    if (!grc_test::RunningUnderWine()) return false;
    BOOST_TEST_MESSAGE("Wine has no Windows security model for pipes and tokens; skipping");
    return true;
}

//! \p nonce_hex with its first digit changed.
std::string OtherNonce(std::string nonce_hex)
{
    nonce_hex[0] = nonce_hex[0] == '0' ? '1' : '0';
    return nonce_hex;
}

} // namespace

// The core and the GUI in one process: the GUI opens the pipe and writes the nonce,
// and the core reads this account as the client, at the identification level.
BOOST_AUTO_TEST_CASE(attest_same_process_roundtrip)
{
    if (SkipUnderWine()) return;
    const Sid self = CurrentTokenUser();

    AttestServer server(self, std::nullopt);
    const std::optional<std::pair<std::string, std::string>> challenge = server.Begin();
    BOOST_REQUIRE(challenge.has_value());
    BOOST_CHECK(IsValidPipeSuffix(challenge->first));
    BOOST_CHECK(IsValidNonceHex(challenge->second));

    // Begin is single use.
    BOOST_CHECK(!server.Begin());

    std::string error;
    std::optional<AttestClientPipe> client = AttestClientPipe::Open(challenge->first, self, error);
    BOOST_REQUIRE_MESSAGE(client.has_value(), error);
    BOOST_REQUIRE_MESSAGE(client->WriteNonce(challenge->second, error), error);

    const std::optional<TokenFacts> facts = server.Finish();
    BOOST_REQUIRE(facts.has_value());
    BOOST_CHECK(facts->user == self);
    BOOST_CHECK_GE(facts->impersonation_level, IMPERSONATION_IDENTIFICATION);
    // The client asked for no more than identification.
    BOOST_CHECK_EQUAL(facts->impersonation_level, IMPERSONATION_IDENTIFICATION);
    BOOST_CHECK(!facts->member_of_attach_group);
    BOOST_CHECK(!facts->is_appcontainer);
    BOOST_TEST_MESSAGE(strprintf("attested %s: integrity 0x%x, impersonation level %u", FormatSid(facts->user),
                                 facts->integrity_rid, facts->impersonation_level));

    // This account is the owner, unless the test runs elevated.
    const Classification c = ClassifyTokenFacts(*facts, self, false);
    BOOST_CHECK(c.cls == (facts->integrity_rid > INTEGRITY_MEDIUM_RID ? PeerClass::Refused : PeerClass::Owner));

    // Finish is single use as well.
    BOOST_CHECK(!server.Finish());
}

// With an attach group configured, the client's membership is read from its token.
// BUILTIN\Users stands in for the group: every interactive account is a member.
BOOST_AUTO_TEST_CASE(attest_reads_attach_group_membership)
{
    if (SkipUnderWine()) return;
    const Sid self = CurrentTokenUser();

    AttestServer server(self, wellknown::Users());
    const std::optional<std::pair<std::string, std::string>> challenge = server.Begin();
    BOOST_REQUIRE(challenge.has_value());

    std::string error;
    std::optional<AttestClientPipe> client = AttestClientPipe::Open(challenge->first, self, error);
    BOOST_REQUIRE_MESSAGE(client.has_value(), error);
    BOOST_REQUIRE_MESSAGE(client->WriteNonce(challenge->second, error), error);

    const std::optional<TokenFacts> facts = server.Finish();
    BOOST_REQUIRE(facts.has_value());
    BOOST_CHECK(facts->user == self);
    BOOST_CHECK(facts->member_of_attach_group);
}

// The pipe carries the access list of AttestPipeAclSpec, and no second instance of
// its name can be created while it is open.
BOOST_AUTO_TEST_CASE(attest_pipe_has_its_access_list)
{
    if (SkipUnderWine()) return;
    const Sid self = CurrentTokenUser();

    AttestServer server(self, wellknown::Users(), std::chrono::seconds{1});
    const std::optional<std::pair<std::string, std::string>> challenge = server.Begin();
    BOOST_REQUIRE(challenge.has_value());
    const std::optional<std::wstring> path = AttestPipePath(challenge->first);
    BOOST_REQUIRE(path.has_value());

    const HANDLE second = ::CreateNamedPipeW(path->c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 64, 64,
                                             0, nullptr);
    BOOST_CHECK(second == INVALID_HANDLE_VALUE);
    if (second != INVALID_HANDLE_VALUE) ::CloseHandle(second);

    const HANDLE handle = ::CreateFileW(path->c_str(), READ_CONTROL | SYNCHRONIZE, 0, nullptr, OPEN_EXISTING,
                                        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
    BOOST_REQUIRE_MESSAGE(handle != INVALID_HANDLE_VALUE, "open failed: " << ::GetLastError());
    std::optional<std::string> mismatch;
    try {
        const ObservedAcl acl = ReadAcl(handle, /*kernel_object=*/true);
        mismatch = CheckMatchesSpec(acl, AttestPipeAclSpec(self, wellknown::Users()));
    } catch (const std::exception& e) {
        mismatch = e.what();
    }
    ::CloseHandle(handle);
    BOOST_CHECK_MESSAGE(!mismatch, mismatch.value_or(""));

    // The client went away without writing anything.
    BOOST_CHECK(!server.Finish());
}

BOOST_AUTO_TEST_CASE(attest_wrong_nonce_refused)
{
    if (SkipUnderWine()) return;
    const Sid self = CurrentTokenUser();

    AttestServer server(self, std::nullopt);
    const std::optional<std::pair<std::string, std::string>> challenge = server.Begin();
    BOOST_REQUIRE(challenge.has_value());

    std::string error;
    std::optional<AttestClientPipe> client = AttestClientPipe::Open(challenge->first, self, error);
    BOOST_REQUIRE_MESSAGE(client.has_value(), error);
    BOOST_REQUIRE_MESSAGE(client->WriteNonce(OtherNonce(challenge->second), error), error);

    BOOST_CHECK(!server.Finish());

    // WriteNonce takes only a valid nonce.
    BOOST_CHECK(!client->WriteNonce(challenge->second.substr(1), error));
    BOOST_CHECK(!error.empty());
}

// Finish waits for no client: one that never connected is refused at once, one that
// connected and wrote nothing within ATTEST_FINISH_WAIT, whatever the deadline. A
// Finish after the deadline is refused even when the nonce was written.
BOOST_AUTO_TEST_CASE(attest_finish_does_not_wait)
{
    if (SkipUnderWine()) return;
    const Sid self = CurrentTokenUser();
    using Clock = std::chrono::steady_clock;

    {
        AttestServer server(self, std::nullopt);
        BOOST_REQUIRE(server.Begin().has_value());
        const Clock::time_point start = Clock::now();
        BOOST_CHECK(!server.Finish());
        BOOST_CHECK(Clock::now() - start < ATTEST_FINISH_WAIT);
    }

    {
        AttestServer server(self, std::nullopt);
        const std::optional<std::pair<std::string, std::string>> challenge = server.Begin();
        BOOST_REQUIRE(challenge.has_value());
        std::string error;
        std::optional<AttestClientPipe> client = AttestClientPipe::Open(challenge->first, self, error);
        BOOST_REQUIRE_MESSAGE(client.has_value(), error);

        const Clock::time_point start = Clock::now();
        BOOST_CHECK(!server.Finish());
        BOOST_CHECK(Clock::now() - start < ATTEST_FINISH_WAIT + std::chrono::milliseconds{500});
    }

    {
        AttestServer server(self, std::nullopt, std::chrono::milliseconds{100});
        const std::optional<std::pair<std::string, std::string>> challenge = server.Begin();
        BOOST_REQUIRE(challenge.has_value());
        std::string error;
        std::optional<AttestClientPipe> client = AttestClientPipe::Open(challenge->first, self, error);
        BOOST_REQUIRE_MESSAGE(client.has_value(), error);
        BOOST_REQUIRE_MESSAGE(client->WriteNonce(challenge->second, error), error);

        std::this_thread::sleep_for(std::chrono::milliseconds{300});
        BOOST_CHECK(!server.Finish());
    }

    // A server begun and destroyed without Finish closes its pipe.
    {
        AttestServer server(self, std::nullopt);
        BOOST_REQUIRE(server.Begin().has_value());
    }
}

// The client reads the pipe's owner before it writes anything, and refuses a pipe
// that is not the expected server's, naming the owner it found.
BOOST_AUTO_TEST_CASE(attest_client_refuses_wrong_owner)
{
    if (SkipUnderWine()) return;
    const Sid self = CurrentTokenUser();

    AttestServer server(self, std::nullopt, std::chrono::seconds{1});
    const std::optional<std::pair<std::string, std::string>> challenge = server.Begin();
    BOOST_REQUIRE(challenge.has_value());

    std::string error;
    BOOST_CHECK(!AttestClientPipe::Open(challenge->first, wellknown::System(), error));
    BOOST_CHECK_MESSAGE(error.find(FormatSid(self)) != std::string::npos, error);

    // Nothing was written, so the server learns nothing.
    BOOST_CHECK(!server.Finish());
}

// A suffix that is not 32 lower-case hex digits is refused before anything is opened.
BOOST_AUTO_TEST_CASE(attest_client_refuses_an_invalid_suffix)
{
    for (const std::string& bad : {std::string("..\\x"), std::string("\\\\host\\pipe\\x"), std::string(32, 'A')}) {
        std::string error;
        BOOST_CHECK(!AttestClientPipe::Open(bad, CurrentTokenUser(), error));
        BOOST_CHECK_MESSAGE(!error.empty(), bad);
    }
}
#endif // WIN32

BOOST_AUTO_TEST_SUITE_END()
