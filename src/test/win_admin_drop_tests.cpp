// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "rpc/client.h"
#include "util/win_acl.h"
#include "util/win_admin_drop.h"
#include "util/winservice.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <ios>
#include <optional>
#include <string>
#include <vector>

#ifdef WIN32
#include "test/wine.h"

#include <windows.h>
#endif

using namespace util::winadmin;
using util::winacl::GROUP_ENABLED;
using util::winacl::GROUP_USE_FOR_DENY_ONLY;
using util::winacl::INTEGRITY_MEDIUM_RID;
using util::winacl::TokenGroup;

namespace {

// SECURITY_MANDATORY_LOW_RID, MEDIUM_PLUS_RID, HIGH_RID and SYSTEM_RID.
constexpr uint32_t LOW = 0x1000;
constexpr uint32_t MEDIUM_PLUS = 0x2100;
constexpr uint32_t HIGH = 0x3000;
constexpr uint32_t SYSTEM = 0x4000;

// What a group of an administrator's full token carries: SE_GROUP_MANDATORY,
// SE_GROUP_ENABLED_BY_DEFAULT and SE_GROUP_ENABLED.
constexpr uint32_t ENABLED = 0x1 | 0x2 | GROUP_ENABLED;
// The logon SID's group: SE_GROUP_LOGON_ID, enabled.
constexpr uint32_t LOGON_ID = 0xC0000000 | ENABLED;
// An integrity label: SE_GROUP_INTEGRITY and SE_GROUP_INTEGRITY_ENABLED.
constexpr uint32_t INTEGRITY_LABEL = 0x20 | 0x40;

//! A domain, standing in for the machine's or a real domain's. Not named DOMAIN:
//! macOS's <math.h> defines that as a macro.
const std::string DOMAIN_SID = "S-1-5-21-1004336348-1177238915-682003330";

Sid SidOf(const std::string& text)
{
    const std::optional<Sid> sid = util::winacl::ParseSidString(text);
    BOOST_REQUIRE_MESSAGE(sid.has_value(), "'" << text << "' does not parse");
    return *sid;
}

TokenGroup Group(const std::string& text, uint32_t attributes)
{
    return {SidOf(text), attributes};
}

//! The facts of a token that needs the drop on every count: elevated, High integrity, and an
//! administrators' group enabled.
StartFacts AdminToken()
{
    StartFacts facts;
    facts.token_elevated = true;
    facts.integrity_rid = HIGH;
    facts.filtered_group_enabled = true;
    return facts;
}

//! The facts of an ordinary desktop token.
StartFacts StandardToken()
{
    return StartFacts{};
}

//! DecideStart for an administrator's token and the command line gridcoinresearchd \p args, read
//! as main() reads it.
StartAction DecideForCommandLine(std::vector<const char*> args)
{
    args.insert(args.begin(), "gridcoinresearchd");
    const int argc = static_cast<int>(args.size());

    StartFacts facts = AdminToken();
    facts.service = util::winsvc::HasServiceFlag(argc, args.data());
    facts.rpc_client = IsRpcClientCommandLine(argc, args.data());
    facts.report_permissions = util::winsvc::CommandLineFlag(argc, args.data(), "reportpermissions");
    return DecideStart(facts);
}

bool IsAscii(const std::string& text)
{
    return std::all_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; });
}

} // namespace

BOOST_AUTO_TEST_SUITE(win_admin_drop_tests)

// The list Microsoft documents for User Account Control's filtered token, by SID: the BUILTIN
// groups, S-1-5-114, and the domain groups by RID on any domain.
BOOST_AUTO_TEST_CASE(filtered_groups_are_the_documented_list)
{
    for (const char* rid : {"544", "547", "548", "549", "550", "551", "553", "554", "556", "569"}) {
        BOOST_CHECK_MESSAGE(IsUacFilteredGroup(SidOf(std::string("S-1-5-32-") + rid)), "S-1-5-32-" << rid);
    }
    BOOST_CHECK(IsUacFilteredGroup(SidOf("S-1-5-114")));
    for (const char* rid : {"498", "512", "516", "517", "518", "519", "520", "521", "553"}) {
        BOOST_CHECK_MESSAGE(IsUacFilteredGroup(SidOf(DOMAIN_SID + "-" + rid)), DOMAIN_SID << "-" << rid);
        // Any domain.
        BOOST_CHECK(IsUacFilteredGroup(SidOf(std::string("S-1-5-21-1-2-3-") + rid)));
    }
}

BOOST_AUTO_TEST_CASE(other_groups_are_not_filtered)
{
    const std::vector<std::string> others{
        "S-1-1-0",                   // Everyone
        "S-1-5-4",                   // INTERACTIVE
        "S-1-5-11",                  // Authenticated Users
        "S-1-5-15",                  // This Organization
        "S-1-5-18",                  // SYSTEM
        "S-1-5-113",                 // Local account
        "S-1-5-5-0-1234567",         // a logon SID
        "S-1-5-32-545",              // Users
        "S-1-5-32-546",              // Guests
        "S-1-5-32-552",              // Replicator
        "S-1-5-32-555",              // Remote Desktop Users
        "S-1-5-32-559",              // Performance Log Users
        "S-1-5-32-573",              // Event Log Readers
        "S-1-16-8192",               // Medium integrity
        "S-1-16-12288",              // High integrity
        DOMAIN_SID + "-500",         // the local Administrator account
        DOMAIN_SID + "-513",         // Domain Users, or None
        DOMAIN_SID + "-1001",        // a user
        // Look-alikes: the RIDs under another authority, the wrong domain form, or the wrong length.
        "S-1-2-32-544",
        "S-1-5-80-544",
        "S-1-5-32-544-1",
        "S-1-5-21-1-2-512",
        "S-1-5-21-1-2-3-4-512",
        "S-1-5-22-1-2-3-512",
        "S-1-5-32-512",
        "S-1-5-21-1-2-3-544",
        "S-1-5-114-1",
        "S-1-16-114",
    };
    for (const std::string& sid : others) {
        BOOST_CHECK_MESSAGE(!IsUacFilteredGroup(SidOf(sid)), sid);
    }

    BOOST_CHECK(!IsUacFilteredGroup(Sid{}));
    // A SID whose count claims more sub-authorities than it holds.
    BOOST_CHECK(!IsUacFilteredGroup(Sid{1, 2, 0, 0, 0, 0, 0, 5, 32, 0, 0, 0}));
}

BOOST_AUTO_TEST_CASE(deny_only_selection_takes_the_enabled_filtered_groups)
{
    const std::vector<TokenGroup> groups{
        Group(DOMAIN_SID + "-1001", ENABLED),    // the user's own SID
        Group("S-1-1-0", ENABLED),               // Everyone
        Group("S-1-5-114", ENABLED),             // Local account and member of Administrators
        Group("S-1-5-32-544", ENABLED),          // Administrators
        Group("S-1-5-32-545", ENABLED),          // Users
        Group("S-1-5-4", ENABLED),               // INTERACTIVE
        Group("S-1-5-11", ENABLED),              // Authenticated Users
        Group("S-1-5-5-0-1234567", LOGON_ID),    // the logon SID
        Group(DOMAIN_SID + "-512", ENABLED),     // Domain Admins
        Group("S-1-5-32-551", 0),                // Backup Operators, disabled
        Group("S-1-5-32-547", GROUP_USE_FOR_DENY_ONLY),                 // Power Users, already deny-only
        Group("S-1-5-32-548", GROUP_USE_FOR_DENY_ONLY | GROUP_ENABLED), // the same, with the enabled bit
        Group("S-1-16-12288", INTEGRITY_LABEL),  // the integrity label
    };

    const std::vector<Sid> selected = SelectDenyOnlyGroups(groups);
    const std::vector<Sid> expected{SidOf("S-1-5-114"), SidOf("S-1-5-32-544"), SidOf(DOMAIN_SID + "-512")};
    BOOST_CHECK(selected == expected);

    BOOST_CHECK(SelectDenyOnlyGroups({}).empty());
    BOOST_CHECK(SelectDenyOnlyGroups({Group(DOMAIN_SID + "-1001", ENABLED), Group("S-1-5-32-545", ENABLED)}).empty());
}

BOOST_AUTO_TEST_CASE(drop_needed_by_elevation_integrity_or_an_enabled_group)
{
    BOOST_CHECK(!DropNeeded(false, INTEGRITY_MEDIUM_RID, false));
    BOOST_CHECK(!DropNeeded(false, LOW, false));
    BOOST_CHECK(!DropNeeded(false, 0, false));

    BOOST_CHECK(DropNeeded(true, INTEGRITY_MEDIUM_RID, false));
    BOOST_CHECK(DropNeeded(false, MEDIUM_PLUS, false));
    BOOST_CHECK(DropNeeded(false, HIGH, false));
    BOOST_CHECK(DropNeeded(false, SYSTEM, false));
    BOOST_CHECK(DropNeeded(false, INTEGRITY_MEDIUM_RID, true));
    BOOST_CHECK(DropNeeded(false, LOW, true));
    BOOST_CHECK(DropNeeded(true, HIGH, true));
}

BOOST_AUTO_TEST_CASE(start_decision_across_its_inputs)
{
    // The service, the RPC client and the report-only permission check keep their token,
    // whatever it is.
    StartFacts facts = AdminToken();
    facts.service = true;
    BOOST_CHECK(DecideStart(facts) == StartAction::Run);
    facts = AdminToken();
    facts.rpc_client = true;
    BOOST_CHECK(DecideStart(facts) == StartAction::Run);
    facts = AdminToken();
    facts.report_permissions = true;
    BOOST_CHECK(DecideStart(facts) == StartAction::Run);

    // A token that needs nothing runs.
    BOOST_CHECK(DecideStart(StandardToken()) == StartAction::Run);
    facts = StandardToken();
    facts.integrity_rid = LOW;
    BOOST_CHECK(DecideStart(facts) == StartAction::Run);

    // Each fact alone needs the drop.
    BOOST_CHECK(DecideStart(AdminToken()) == StartAction::Drop);
    facts = StandardToken();
    facts.token_elevated = true;
    BOOST_CHECK(DecideStart(facts) == StartAction::Drop);
    for (const uint32_t rid : {MEDIUM_PLUS, HIGH, SYSTEM}) {
        facts = StandardToken();
        facts.integrity_rid = rid;
        BOOST_CHECK_MESSAGE(DecideStart(facts) == StartAction::Drop, "integrity 0x" << std::hex << rid);
    }
    facts = StandardToken();
    facts.filtered_group_enabled = true;
    BOOST_CHECK(DecideStart(facts) == StartAction::Drop);
}

BOOST_AUTO_TEST_CASE(loop_guard_refuses_only_a_copy_that_still_needs_the_drop)
{
    // The marker set and the drop still needed: refuse, never start again.
    StartFacts facts = AdminToken();
    facts.rights_dropped_marker = true;
    BOOST_CHECK(DecideStart(facts) == StartAction::Refuse);
    facts = StandardToken();
    facts.rights_dropped_marker = true;
    facts.integrity_rid = HIGH;
    BOOST_CHECK(DecideStart(facts) == StartAction::Refuse);
    facts = StandardToken();
    facts.rights_dropped_marker = true;
    facts.filtered_group_enabled = true;
    BOOST_CHECK(DecideStart(facts) == StartAction::Refuse);

    // The marker set and nothing needed: run; the marker gives nothing.
    facts = StandardToken();
    facts.rights_dropped_marker = true;
    BOOST_CHECK(DecideStart(facts) == StartAction::Run);

    // The service, the RPC client and the permission check do not look at it.
    facts = AdminToken();
    facts.rights_dropped_marker = true;
    facts.service = true;
    BOOST_CHECK(DecideStart(facts) == StartAction::Run);
    facts = AdminToken();
    facts.rights_dropped_marker = true;
    facts.rpc_client = true;
    BOOST_CHECK(DecideStart(facts) == StartAction::Run);
    facts = AdminToken();
    facts.rights_dropped_marker = true;
    facts.report_permissions = true;
    BOOST_CHECK(DecideStart(facts) == StartAction::Run);
}

// The service flag and the RPC client form are read from the command line as main() and AppInit
// read them. -help and -version are a core's command line, so they drop too.
BOOST_AUTO_TEST_CASE(command_line_decides_service_and_client_form)
{
    BOOST_CHECK(DecideForCommandLine({}) == StartAction::Drop);
    BOOST_CHECK(DecideForCommandLine({"-testnet"}) == StartAction::Drop);
    BOOST_CHECK(DecideForCommandLine({"-datadir=C:\\gridcoin", "-multiprocess"}) == StartAction::Drop);
    BOOST_CHECK(DecideForCommandLine({"-help"}) == StartAction::Drop);
    BOOST_CHECK(DecideForCommandLine({"-?"}) == StartAction::Drop);
    BOOST_CHECK(DecideForCommandLine({"-version"}) == StartAction::Drop);
    BOOST_CHECK(DecideForCommandLine({"-service=0"}) == StartAction::Drop);
    // A word that starts with the program's name is not a command.
    BOOST_CHECK(DecideForCommandLine({"gridcoinresearchd.exe"}) == StartAction::Drop);
    BOOST_CHECK(DecideForCommandLine({"GridcoinResearchD", "-testnet"}) == StartAction::Drop);

    BOOST_CHECK(DecideForCommandLine({"-service"}) == StartAction::Run);
    BOOST_CHECK(DecideForCommandLine({"-testnet", "-service"}) == StartAction::Run);
    BOOST_CHECK(DecideForCommandLine({"getinfo"}) == StartAction::Run);
    BOOST_CHECK(DecideForCommandLine({"-testnet", "getinfo"}) == StartAction::Run);
    BOOST_CHECK(DecideForCommandLine({"-datadir=C:\\gridcoin", "stop"}) == StartAction::Run);
    BOOST_CHECK(DecideForCommandLine({"help", "getinfo"}) == StartAction::Run);

    const std::vector<const char*> client{"gridcoinresearchd", "-testnet", "getblockcount"};
    BOOST_CHECK(IsRpcClientCommandLine(static_cast<int>(client.size()), client.data()));
    const std::vector<const char*> node{"gridcoinresearchd", "-testnet", "-printtoconsole"};
    BOOST_CHECK(!IsRpcClientCommandLine(static_cast<int>(node.size()), node.data()));
    // The first argument is the program, whatever it holds.
    const std::vector<const char*> program_only{"getinfo"};
    BOOST_CHECK(!IsRpcClientCommandLine(static_cast<int>(program_only.size()), program_only.data()));
}

// -reportpermissions, which the installer runs elevated, starts no core: it keeps its token. It
// is read as ArgsManager reads a boolean option on Windows, the last one counting.
BOOST_AUTO_TEST_CASE(command_line_permission_report_keeps_its_token)
{
    BOOST_CHECK(DecideForCommandLine({"-reportpermissions"}) == StartAction::Run);
    BOOST_CHECK(DecideForCommandLine({"-testnet", "-reportpermissions"}) == StartAction::Run);
    BOOST_CHECK(DecideForCommandLine({"--reportpermissions"}) == StartAction::Run);
    BOOST_CHECK(DecideForCommandLine({"-reportpermissions=1"}) == StartAction::Run);
    BOOST_CHECK(DecideForCommandLine({"-noreportpermissions=0"}) == StartAction::Run);

    BOOST_CHECK(DecideForCommandLine({"-reportpermissions=0"}) == StartAction::Drop);
    BOOST_CHECK(DecideForCommandLine({"-noreportpermissions"}) == StartAction::Drop);
    BOOST_CHECK(DecideForCommandLine({"-reportpermissions", "-noreportpermissions"}) == StartAction::Drop);
    BOOST_CHECK(DecideForCommandLine({"-reportpermissionsx"}) == StartAction::Drop);
    // After the first argument that is not an option, the rest belong to a command.
    BOOST_CHECK(DecideForCommandLine({"-", "-reportpermissions"}) == StartAction::Drop);

    // "/" starts an option as "-" does, as on Windows, where the drop runs. (Elsewhere the RPC
    // client form reads "/ReportPermissions" as a command.)
    const std::vector<const char*> slash{"gridcoinresearchd", "/ReportPermissions"};
    BOOST_CHECK(util::winsvc::CommandLineFlag(static_cast<int>(slash.size()), slash.data(), "reportpermissions"));
    const std::vector<const char*> other{"gridcoinresearchd", "-service"};
    BOOST_CHECK(!util::winsvc::CommandLineFlag(static_cast<int>(other.size()), other.data(), "reportpermissions"));
}

BOOST_AUTO_TEST_CASE(refusal_names_the_reason_and_ends_with_the_advice)
{
    const std::string reason = "CreateRestrictedToken failed with Windows error 5";
    const std::string message = RefusalMessage(reason);
    const std::string advice = "Start Gridcoin from a normal (not elevated) session.";

    BOOST_CHECK(message.find(reason) != std::string::npos);
    BOOST_REQUIRE_GE(message.size(), advice.size());
    BOOST_CHECK_EQUAL(message.substr(message.size() - advice.size()), advice);
    BOOST_CHECK(message.find('\n') == std::string::npos);
    BOOST_CHECK(IsAscii(message));
}

BOOST_AUTO_TEST_CASE(marker_and_log_line_texts)
{
    BOOST_CHECK_EQUAL(std::string(RIGHTS_DROPPED_ENV), "GRIDCOIN_ADMIN_RIGHTS_DROPPED");
    BOOST_CHECK_EQUAL(std::string(STARTED_WITHOUT_ADMIN_RIGHTS),
                      "Started without administrator rights: the process that started Gridcoin had them.");
    BOOST_CHECK(IsAscii(STARTED_WITHOUT_ADMIN_RIGHTS));
}

#ifdef WIN32
namespace {

using grc_test::RunningUnderWine;
using util::winacl::ObservedAce;
using util::winacl::UniqueHandle;

//! The path of a file in the system directory.
std::wstring SystemProgram(const wchar_t* name)
{
    std::wstring system(MAX_PATH, L'\0');
    const UINT length = ::GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size()));
    BOOST_REQUIRE(length > 0 && length < system.size());
    system.resize(length);
    return system + L"\\" + name;
}

//! This test binary's path.
std::wstring ThisProgram()
{
    std::wstring module(32768, L'\0');
    const DWORD length = ::GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    BOOST_REQUIRE(length > 0 && length < module.size());
    module.resize(length);
    return module;
}

//! The names of the privileges of \p token.
std::vector<std::wstring> PrivilegeNames(HANDLE token)
{
    const std::vector<unsigned char> buffer =
        util::winacl::QueryTokenInformation(token, TokenPrivileges, "TokenPrivileges");
    const auto* const privileges = reinterpret_cast<const TOKEN_PRIVILEGES*>(buffer.data());

    std::vector<std::wstring> names;
    for (DWORD i = 0; i < privileges->PrivilegeCount; ++i) {
        LUID luid = privileges->Privileges[i].Luid;
        wchar_t name[128];
        DWORD length = sizeof(name) / sizeof(name[0]);
        BOOST_REQUIRE(::LookupPrivilegeNameW(nullptr, &luid, name, &length));
        names.emplace_back(name, length);
    }
    return names;
}

//! The entries of \p acl, in order.
std::vector<ObservedAce> Entries(const ACL* acl)
{
    std::vector<ObservedAce> entries;
    const auto* const begin = reinterpret_cast<const unsigned char*>(acl);
    const auto* const end = begin + acl->AclSize;
    for (DWORD i = 0; i < acl->AceCount; ++i) {
        void* entry = nullptr;
        BOOST_REQUIRE(::GetAce(const_cast<ACL*>(acl), i, &entry));
        const auto* const bytes = static_cast<const unsigned char*>(entry);
        const std::optional<ObservedAce> ace = util::winacl::ParseAce(bytes, static_cast<size_t>(end - bytes));
        BOOST_REQUIRE(ace.has_value());
        entries.push_back(*ace);
    }
    return entries;
}

//! Whether \p entries are exactly an allow entry with \p mask for \p user, then one for SYSTEM,
//! neither inherited nor inheritable.
bool UserAndSystemOnly(const std::vector<ObservedAce>& entries, const Sid& user, uint32_t mask)
{
    const auto is = [mask](const ObservedAce& ace, const Sid& trustee) {
        return ace.kind == util::winacl::AceKind::Allow && ace.trustee == trustee && ace.mask == mask &&
               !ace.inherited && !ace.inherit_only && ace.inherit == util::winacl::Inherit::None;
    };
    return entries.size() == 2 && is(entries[0], user) && is(entries[1], util::winacl::wellknown::System());
}

//! Whether \p entries are an allow entry with \p mask for \p user, then one for SYSTEM, and then at most one
//! allow entry for BUILTIN\Administrators whose mask is exactly TOKEN_QUERY, none inherited or inheritable.
bool UserSystemAndAdministratorsQuery(const std::vector<ObservedAce>& entries, const Sid& user, uint32_t mask)
{
    if (entries.size() < 2 || entries.size() > 3) return false;
    if (!UserAndSystemOnly({entries.begin(), entries.begin() + 2}, user, mask)) return false;
    if (entries.size() == 2) return true;
    const ObservedAce& ace = entries[2];
    return ace.kind == util::winacl::AceKind::Allow && ace.trustee == util::winacl::wellknown::Administrators() &&
           ace.mask == TOKEN_QUERY && !ace.inherited && !ace.inherit_only &&
           ace.inherit == util::winacl::Inherit::None;
}

//! Checks that \p token is the restricted copy of this process's token: every group of the
//! filtered list that was enabled in \p original is deny-only and none of that list is enabled;
//! the only privilege is SeChangeNotifyPrivilege; no restricting SIDs; Medium integrity, not
//! elevated, so the copy does not need the drop again; the owner is \p user; the default DACL is
//! \p user and SYSTEM with GENERIC_ALL; and the token object itself grants them all access, besides
//! the query entry Windows adds for Administrators.
void CheckRestrictedToken(HANDLE token, const Sid& user, const std::vector<TokenGroup>& original)
{
    const std::vector<TokenGroup> groups = util::winacl::ReadTokenGroups(token);
    for (const Sid& sid : SelectDenyOnlyGroups(original)) {
        const auto found =
            std::find_if(groups.begin(), groups.end(), [&sid](const TokenGroup& g) { return g.sid == sid; });
        BOOST_REQUIRE_MESSAGE(found != groups.end(), util::winacl::FormatSid(sid) << " is gone from the token");
        BOOST_CHECK_MESSAGE((found->attributes & GROUP_USE_FOR_DENY_ONLY) != 0,
                            util::winacl::FormatSid(sid) << " is not deny-only");
    }
    for (const TokenGroup& group : groups) {
        if (!IsUacFilteredGroup(group.sid)) continue;
        const bool enabled =
            (group.attributes & GROUP_ENABLED) != 0 && (group.attributes & GROUP_USE_FOR_DENY_ONLY) == 0;
        BOOST_CHECK_MESSAGE(!enabled, util::winacl::FormatSid(group.sid)
                                          << " is still enabled, attributes 0x" << std::hex << group.attributes);
    }
    BOOST_CHECK(SelectDenyOnlyGroups(groups).empty());

    const std::vector<std::wstring> privileges = PrivilegeNames(token);
    BOOST_CHECK_EQUAL(privileges.size(), 1U);
    BOOST_CHECK(privileges.size() == 1 && privileges[0] == L"SeChangeNotifyPrivilege");

    const std::vector<unsigned char> restricting =
        util::winacl::QueryTokenInformation(token, TokenRestrictedSids, "TokenRestrictedSids");
    BOOST_CHECK_EQUAL(reinterpret_cast<const TOKEN_GROUPS*>(restricting.data())->GroupCount, 0U);

    const uint32_t integrity = util::winacl::ReadTokenIntegrityRid(token);
    BOOST_CHECK_EQUAL(integrity, INTEGRITY_MEDIUM_RID);
    const bool elevated = util::winacl::ReadTokenElevated(token);
    BOOST_CHECK(!elevated);
    BOOST_CHECK(!DropNeeded(elevated, integrity, !SelectDenyOnlyGroups(groups).empty()));

    BOOST_CHECK(util::winacl::ReadTokenUser(token) == user);
    const std::vector<unsigned char> owner = util::winacl::QueryTokenInformation(token, TokenOwner, "TokenOwner");
    BOOST_CHECK(util::winacl::SidAt(reinterpret_cast<const TOKEN_OWNER*>(owner.data())->Owner) == user);

    const std::vector<unsigned char> dacl =
        util::winacl::QueryTokenInformation(token, TokenDefaultDacl, "TokenDefaultDacl");
    const ACL* const default_dacl = reinterpret_cast<const TOKEN_DEFAULT_DACL*>(dacl.data())->DefaultDacl;
    BOOST_REQUIRE(default_dacl != nullptr);
    BOOST_CHECK(UserAndSystemOnly(Entries(default_dacl), user, GENERIC_ALL));

    const util::winacl::ObservedAcl object = util::winacl::ReadAcl(token, /*kernel_object=*/true);
    BOOST_CHECK(object.dacl_present);
    // Windows adds an entry granting BUILTIN\Administrators TOKEN_QUERY to the primary token of a process.
    // Administrators is deny-only in the copy, and TOKEN_QUERY lets an elevated administrator read token
    // information only.
    BOOST_CHECK(UserSystemAndAdministratorsQuery(object.aces, user, TOKEN_ALL_ACCESS));
}

//! \p program with \p arguments, started under \p token and suspended.
RestrictedChild StartChild(const UniqueHandle& token, const std::wstring& program, const std::wstring& arguments)
{
    try {
        return StartRestrictedSuspended(token.get(), program, L"\"" + program + L"\" " + arguments);
    } catch (const std::exception& e) {
        BOOST_FAIL("StartRestrictedSuspended: " << e.what());
    }
    return {};
}

UniqueHandle RestrictedToken()
{
    try {
        return CreateRestrictedProcessToken();
    } catch (const std::exception& e) {
        BOOST_FAIL("CreateRestrictedProcessToken: " << e.what());
    }
    return {};
}

} // namespace

// Run both elevated and as a standard user: elevated, the administrators' groups are enabled in
// this process's token and must be deny-only in the copy; as a standard user there may be none.
BOOST_AUTO_TEST_CASE(restricted_token_is_the_filtered_copy_of_this_token)
{
    if (RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine's tokens carry no Windows groups or integrity levels; this case runs on Windows");
        return;
    }

    const std::vector<TokenGroup> original = util::winacl::CurrentTokenGroups();
    BOOST_TEST_MESSAGE("this process: " << (util::winacl::CurrentTokenIsElevated() ? "elevated" : "not elevated")
                       << ", integrity 0x" << std::hex << util::winacl::CurrentTokenIntegrityRid() << std::dec
                       << ", " << SelectDenyOnlyGroups(original).size() << " filtered group(s) enabled");

    const UniqueHandle token = RestrictedToken();
    CheckRestrictedToken(token.get(), util::winacl::CurrentTokenUser(), original);
}

// The copy is a process under that token, read through its process handle, held in a job that
// kills it when the job closes, as the starting process's exit closes it.
BOOST_AUTO_TEST_CASE(restricted_child_runs_under_the_token_and_ends_with_its_job)
{
    if (RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine's tokens carry no Windows groups or integrity levels; this case runs on Windows");
        return;
    }

    const std::vector<TokenGroup> original = util::winacl::CurrentTokenGroups();
    const UniqueHandle token = RestrictedToken();
    RestrictedChild child = StartChild(token, ThisProgram(), L"--list_content");

    HANDLE opened = nullptr;
    BOOST_REQUIRE(::OpenProcessToken(child.process.get(), TOKEN_QUERY | READ_CONTROL, &opened));
    const UniqueHandle child_token(opened);
    CheckRestrictedToken(child_token.get(), util::winacl::CurrentTokenUser(), original);

    BOOL in_job = FALSE;
    BOOST_REQUIRE(::IsProcessInJob(child.process.get(), child.job.get(), &in_job));
    BOOST_CHECK(in_job);

    // Never resumed: closing the job ends it.
    BOOST_CHECK_EQUAL(::WaitForSingleObject(child.process.get(), 0), static_cast<DWORD>(WAIT_TIMEOUT));
    child.job.reset();
    BOOST_CHECK_EQUAL(::WaitForSingleObject(child.process.get(), 10000), static_cast<DWORD>(WAIT_OBJECT_0));
}

// The copy can read its own token, as the core does at its start (whoami.exe opens its process
// token and fails without it), and its exit code comes back.
BOOST_AUTO_TEST_CASE(restricted_child_reads_its_own_token_and_its_exit_code_comes_back)
{
    if (RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine's tokens carry no Windows groups or integrity levels; this case runs on Windows");
        return;
    }

    const UniqueHandle token = RestrictedToken();

    RestrictedChild whoami = StartChild(token, SystemProgram(L"whoami.exe"), L"/groups");
    BOOST_CHECK_EQUAL(ResumeAndWait(whoami), 0U);

    RestrictedChild cmd = StartChild(token, SystemProgram(L"cmd.exe"), L"/d /c exit 7");
    BOOST_CHECK_EQUAL(ResumeAndWait(cmd), 7U);
}
#endif // WIN32

BOOST_AUTO_TEST_SUITE_END()
