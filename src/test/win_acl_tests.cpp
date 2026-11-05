// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/win_acl.h"

#include <boost/test/unit_test.hpp>

#include <optional>
#include <string>
#include <vector>

#ifdef WIN32
#include "util/dir_permissions.h"

#include <windows.h>
#endif

using namespace util::winacl;

// The mask constants by their bare names. On Windows <windows.h> defines each of them
// as a macro of the same value (win_acl.cpp checks that), so these cases read the same
// on every platform.
using namespace util::winacl::mask;

namespace {

Sid SidOf(const std::string& text)
{
    const std::optional<Sid> sid = ParseSidString(text);
    BOOST_REQUIRE_MESSAGE(sid.has_value(), "'" << text << "' does not parse");
    return *sid;
}

//! A local account, standing in for the token user.
Sid Self()
{
    return SidOf("S-1-5-21-1004336348-1177238915-682003330-1001");
}

Sid AuthenticatedUsers()
{
    return SidOf("S-1-5-11");
}

ObservedAce Ace(AceKind kind, const Sid& trustee, uint32_t access, Inherit inherit = Inherit::None,
                bool inherit_only = false)
{
    ObservedAce ace;
    ace.kind = kind;
    ace.trustee = trustee;
    ace.mask = access;
    ace.inherit_only = inherit_only;
    ace.inherit = inherit;

    return ace;
}

//! A protected DACL owned by \p owner, holding \p aces.
ObservedAcl Acl(const Sid& owner, std::vector<ObservedAce> aces)
{
    ObservedAcl acl;
    acl.owner = owner;
    acl.dacl_present = true;
    acl.is_protected = true;
    acl.aces = std::move(aces);

    return acl;
}

bool Contains(const std::optional<std::string>& reason, const std::string& text)
{
    return reason.has_value() && reason->find(text) != std::string::npos;
}

} // namespace

BOOST_AUTO_TEST_SUITE(win_acl_tests)

BOOST_AUTO_TEST_CASE(sid_string_roundtrip)
{
    for (const char* text : {"S-1-5-18", "S-1-5-32-544",
                             "S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464"}) {
        const std::optional<Sid> sid = ParseSidString(text);
        BOOST_REQUIRE_MESSAGE(sid.has_value(), "'" << text << "' does not parse");
        BOOST_CHECK_EQUAL(FormatSid(*sid), text);
    }

    for (const char* text : {"", "S-1", "S-1-x-18", "S-1-5-18-", "S-1-5"}) {
        BOOST_CHECK_MESSAGE(!ParseSidString(text).has_value(), "'" << text << "' parses");
    }
}

BOOST_AUTO_TEST_CASE(sid_binary_form_matches_windows)
{
    // S-1-5-32-544 as Windows stores it: revision, two sub-authorities, the authority
    // big-endian, then each sub-authority little-endian.
    const Sid administrators{0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
                             0x20, 0x00, 0x00, 0x00, 0x20, 0x02, 0x00, 0x00};
    BOOST_CHECK(ParseSidString("S-1-5-32-544") == administrators);
    BOOST_CHECK(wellknown::Administrators() == administrators);

    BOOST_CHECK_EQUAL(FormatSid(wellknown::System()), "S-1-5-18");
    BOOST_CHECK_EQUAL(FormatSid(wellknown::Users()), "S-1-5-32-545");
    BOOST_CHECK_EQUAL(FormatSid(wellknown::AnonymousLogon()), "S-1-5-7");
    BOOST_CHECK_EQUAL(FormatSid(wellknown::TrustedInstaller()),
                      "S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464");

    // An authority of 2^32 or more is written in hex, as ConvertSidToStringSid writes
    // it, and 2^48 does not fit.
    BOOST_CHECK_EQUAL(FormatSid(SidOf("S-1-0x123456789ABC-7")), "S-1-0x123456789ABC-7");
    BOOST_CHECK_EQUAL(FormatSid(SidOf("S-1-4294967296-7")), "S-1-0x000100000000-7");
    BOOST_CHECK(!ParseSidString("S-1-281474976710656-7").has_value());

    // Fifteen sub-authorities is the most a SID holds, and each is 32 bits.
    BOOST_CHECK(ParseSidString("S-1-5-1-2-3-4-5-6-7-8-9-10-11-12-13-14-15").has_value());
    BOOST_CHECK(!ParseSidString("S-1-5-1-2-3-4-5-6-7-8-9-10-11-12-13-14-15-16").has_value());
    BOOST_CHECK(ParseSidString("S-1-5-4294967295").has_value());
    BOOST_CHECK(!ParseSidString("S-1-5-4294967296").has_value());
    BOOST_CHECK(!ParseSidString("S-2-5-18").has_value());
    BOOST_CHECK(!ParseSidString("S-1-5--18").has_value());
    BOOST_CHECK(!ParseSidString("S-1-5-+18").has_value());

    // A binary SID whose length disagrees with its count has no string form.
    BOOST_CHECK_EQUAL(FormatSid(Sid{0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x20, 0x00, 0x00, 0x00}), "");
    BOOST_CHECK_EQUAL(FormatSid(Sid{}), "");
}

BOOST_AUTO_TEST_CASE(foreign_writer_counts_other_allow_kinds)
{
    // A callback, object or conditional allow entry grants as a plain one does.
    const ObservedAcl acl =
        Acl(Self(), {ObservedAce{AceKind::OtherAllow, wellknown::Users(), FILE_WRITE_DATA, false, false}});

    const std::optional<std::string> reason = FindForeignWriter(acl, {Self(), wellknown::System()}, FILE_WRITEISH);
    BOOST_REQUIRE(reason.has_value());
    BOOST_CHECK(!reason->empty());
    BOOST_CHECK_MESSAGE(Contains(reason, "S-1-5-32-545"), reason.value_or(""));
}

BOOST_AUTO_TEST_CASE(foreign_writer_skips_deny_and_inherit_only)
{
    const ObservedAcl acl = Acl(Self(), {
        Ace(AceKind::Allow, Self(), FILE_ALL_ACCESS),
        Ace(AceKind::Deny, wellknown::Users(), FILE_ALL_ACCESS),
        Ace(AceKind::Allow, wellknown::Users(), GENERIC_ALL, Inherit::ContainerAndObject, /*inherit_only=*/true),
    });

    const std::optional<std::string> reason = FindForeignWriter(acl, {Self(), wellknown::System()}, FILE_WRITEISH);
    BOOST_CHECK_MESSAGE(!reason.has_value(), reason.value_or(""));
}

BOOST_AUTO_TEST_CASE(foreign_writer_flags_missing_dacl)
{
    ObservedAcl acl = Acl(Self(), {});
    acl.dacl_present = false;

    const std::optional<std::string> reason = FindForeignWriter(acl, {Self(), wellknown::System()}, FILE_WRITEISH);
    BOOST_CHECK_MESSAGE(Contains(reason, "no DACL"), reason.value_or("(none)"));
}

BOOST_AUTO_TEST_CASE(foreign_writer_flags_foreign_owner)
{
    // The owner can rewrite the DACL whatever it says.
    const ObservedAcl acl = Acl(wellknown::Users(), {Ace(AceKind::Allow, Self(), FILE_ALL_ACCESS)});

    const std::optional<std::string> reason = FindForeignWriter(acl, {Self(), wellknown::System()}, FILE_WRITEISH);
    BOOST_CHECK_MESSAGE(Contains(reason, "owned by"), reason.value_or("(none)"));

    // An owner that cannot be read is not taken to be an allowed one.
    ObservedAcl unowned = acl;
    unowned.owner.reset();
    BOOST_CHECK(FindForeignWriter(unowned, {Self(), wellknown::System()}, FILE_WRITEISH).has_value());
}

BOOST_AUTO_TEST_CASE(chain_mask_accepts_volume_root_defaults)
{
    // C:\ as Windows 10 sets it up. Every account may create a directory there, which
    // cannot replace an existing entry, and the Modify grant reaches only what is
    // created below.
    const ObservedAcl root = Acl(wellknown::TrustedInstaller(), {
        Ace(AceKind::Allow, wellknown::Administrators(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
        Ace(AceKind::Allow, wellknown::System(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
        Ace(AceKind::Allow, wellknown::Users(), 0x1200A9, Inherit::ContainerAndObject),
        Ace(AceKind::Allow, AuthenticatedUsers(), FILE_ADD_SUBDIRECTORY),
        Ace(AceKind::Allow, AuthenticatedUsers(), 0x1301BF, Inherit::ContainerAndObject, /*inherit_only=*/true),
    });
    const std::vector<Sid> allowed{wellknown::System(), wellknown::Administrators(), wellknown::TrustedInstaller()};

    const std::optional<std::string> chain = FindForeignWriter(root, allowed, CHAIN_DIR_WRITEISH);
    BOOST_CHECK_MESSAGE(!chain.has_value(), chain.value_or(""));

    // An IPC directory may not let anyone else add an entry of any kind.
    const std::optional<std::string> ipc = FindForeignWriter(root, allowed, IPC_DIR_WRITEISH);
    BOOST_CHECK_MESSAGE(Contains(ipc, "S-1-5-11"), ipc.value_or("(none)"));
}

BOOST_AUTO_TEST_CASE(chain_mask_accepts_program_files_defaults)
{
    // C:\Program Files as Windows 10 sets it up.
    const ObservedAcl program_files = Acl(wellknown::TrustedInstaller(), {
        Ace(AceKind::Allow, wellknown::TrustedInstaller(), FILE_ALL_ACCESS),
        Ace(AceKind::Allow, wellknown::System(), 0x1301BF),
        Ace(AceKind::Allow, wellknown::Administrators(), 0x1301BF),
        Ace(AceKind::Allow, wellknown::Users(), 0x1200A9),
        Ace(AceKind::Allow, SidOf("S-1-3-0"), GENERIC_ALL, Inherit::ContainerAndObject, /*inherit_only=*/true),
        Ace(AceKind::Allow, SidOf("S-1-15-2-1"), 0x1200A9),
        Ace(AceKind::Allow, SidOf("S-1-15-2-2"), 0x1200A9),
    });
    const std::vector<Sid> allowed{wellknown::System(), wellknown::Administrators(), wellknown::TrustedInstaller()};

    const std::optional<std::string> reason = FindForeignWriter(program_files, allowed, CHAIN_DIR_WRITEISH);
    BOOST_CHECK_MESSAGE(!reason.has_value(), reason.value_or(""));

    // The same directory with Users given Modify is not a safe ancestor.
    ObservedAcl widened = program_files;
    widened.aces.push_back(Ace(AceKind::Allow, wellknown::Users(), 0x1301BF));
    BOOST_CHECK(FindForeignWriter(widened, allowed, CHAIN_DIR_WRITEISH).has_value());
}

BOOST_AUTO_TEST_CASE(foreign_reader_flags_read_grants)
{
    const std::vector<Sid> allowed{Self(), wellknown::System()};

    // Reading the ACL and the attributes is not reading the contents.
    const ObservedAcl metadata =
        Acl(Self(), {Ace(AceKind::Allow, wellknown::Users(), READ_CONTROL | FILE_READ_ATTRIBUTES)});
    const std::optional<std::string> none = FindForeignReader(metadata, allowed);
    BOOST_CHECK_MESSAGE(!none.has_value(), none.value_or(""));

    for (const uint32_t access : {static_cast<uint32_t>(FILE_READ_DATA), static_cast<uint32_t>(GENERIC_READ),
                                  static_cast<uint32_t>(GENERIC_ALL)}) {
        const ObservedAcl acl = Acl(Self(), {Ace(AceKind::OtherAllow, wellknown::Users(), access)});
        BOOST_CHECK_MESSAGE(FindForeignReader(acl, allowed).has_value(), "mask " << access);
    }
}

BOOST_AUTO_TEST_CASE(foreign_trustee_counts_inherit_only_entries)
{
    const std::vector<Sid> allowed{Self(), wellknown::System()};
    const ObservedAcl owner_only = Acl(Self(), {
        Ace(AceKind::Allow, Self(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
        Ace(AceKind::Allow, wellknown::System(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
        Ace(AceKind::Deny, wellknown::Users(), FILE_ALL_ACCESS),
    });
    const std::optional<std::string> clean = FindForeignTrustee(owner_only, allowed);
    BOOST_CHECK_MESSAGE(!clean.has_value(), clean.value_or(""));

    // On a directory an inherit-only entry is what everything created inside receives.
    ObservedAcl inherits = owner_only;
    inherits.aces.push_back(Ace(AceKind::Allow, wellknown::Users(), GENERIC_ALL, Inherit::ContainerAndObject,
                                /*inherit_only=*/true));
    BOOST_CHECK(FindForeignTrustee(inherits, allowed).has_value());

    // Any allow-type entry naming another trustee counts, whatever it grants.
    ObservedAcl callback = owner_only;
    callback.aces.push_back(Ace(AceKind::OtherAllow, wellknown::Users(), 0));
    BOOST_CHECK(FindForeignTrustee(callback, allowed).has_value());

    ObservedAcl foreign_owner = owner_only;
    foreign_owner.owner = wellknown::Administrators();
    BOOST_CHECK(Contains(FindForeignTrustee(foreign_owner, allowed), "owned by"));

    ObservedAcl no_dacl = owner_only;
    no_dacl.dacl_present = false;
    BOOST_CHECK(Contains(FindForeignTrustee(no_dacl, allowed), "no DACL"));
}

BOOST_AUTO_TEST_CASE(matches_spec_requires_protection_owner_and_exact_aces)
{
    const AclSpec spec{Self(), {
        {Self(), FILE_ALL_ACCESS, Inherit::ContainerAndObject},
        {wellknown::System(), FILE_ALL_ACCESS, Inherit::ContainerAndObject},
    }};
    // The order of the entries does not matter.
    const ObservedAcl exact = Acl(Self(), {
        Ace(AceKind::Allow, wellknown::System(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
        Ace(AceKind::Allow, Self(), FILE_ALL_ACCESS, Inherit::ContainerAndObject),
    });
    const std::optional<std::string> clean = CheckMatchesSpec(exact, spec);
    BOOST_CHECK_MESSAGE(!clean.has_value(), clean.value_or(""));

    ObservedAcl unprotected = exact;
    unprotected.is_protected = false;
    BOOST_CHECK(Contains(CheckMatchesSpec(unprotected, spec), "not protected"));

    ObservedAcl no_dacl = exact;
    no_dacl.dacl_present = false;
    BOOST_CHECK(Contains(CheckMatchesSpec(no_dacl, spec), "no DACL"));

    ObservedAcl other_owner = exact;
    other_owner.owner = wellknown::Administrators();
    BOOST_CHECK(Contains(CheckMatchesSpec(other_owner, spec), "owned by"));

    ObservedAcl extra_allow = exact;
    extra_allow.aces.push_back(Ace(AceKind::Allow, wellknown::Users(), FILE_READ_DATA));
    BOOST_CHECK(CheckMatchesSpec(extra_allow, spec).has_value());

    ObservedAcl extra_other_allow = exact;
    extra_other_allow.aces.push_back(Ace(AceKind::OtherAllow, Self(), FILE_ALL_ACCESS, Inherit::ContainerAndObject));
    BOOST_CHECK(CheckMatchesSpec(extra_other_allow, spec).has_value());

    ObservedAcl other_mask = exact;
    other_mask.aces[1].mask = GENERIC_ALL;
    BOOST_CHECK(CheckMatchesSpec(other_mask, spec).has_value());

    ObservedAcl other_inherit = exact;
    other_inherit.aces[1].inherit = Inherit::None;
    BOOST_CHECK(CheckMatchesSpec(other_inherit, spec).has_value());

    // Inheritance the spec cannot state (object-only, container-only, no-propagate).
    ObservedAcl partial_inherit = exact;
    partial_inherit.aces[1].inherit = std::nullopt;
    BOOST_CHECK(CheckMatchesSpec(partial_inherit, spec).has_value());

    ObservedAcl inherit_only = exact;
    inherit_only.aces[1].inherit_only = true;
    BOOST_CHECK(CheckMatchesSpec(inherit_only, spec).has_value());

    ObservedAcl missing = exact;
    missing.aces.pop_back();
    BOOST_CHECK(CheckMatchesSpec(missing, spec).has_value());

    // A deny entry, or an entry of another kind, only narrows.
    ObservedAcl extra_deny = exact;
    extra_deny.aces.push_back(Ace(AceKind::Deny, wellknown::Users(), FILE_ALL_ACCESS));
    extra_deny.aces.push_back(Ace(AceKind::Other, Sid{}, 0));
    const std::optional<std::string> narrowed = CheckMatchesSpec(extra_deny, spec);
    BOOST_CHECK_MESSAGE(!narrowed.has_value(), narrowed.value_or(""));
}

#ifdef WIN32

namespace {

//! Wine accepts an access control list without keeping its protection, so the cases
//! that apply one hold only on Windows itself. The cross-compiled suite also runs under
//! Wine, where they say so and return.
bool RunningUnderWine()
{
    const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");

    return ntdll != nullptr && ::GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

//! A fresh directory under the system temp directory, removed when the case ends.
struct TempDir
{
    fs::path path{fs::temp_directory_path() / fs::unique_path("win_acl_%%%%-%%%%-%%%%")};

    TempDir()
    {
        fs::create_directories(path);
    }

    ~TempDir()
    {
        boost::system::error_code ec;
        fs::remove_all(path, ec);
    }
};

} // namespace

BOOST_AUTO_TEST_CASE(apply_acl_roundtrip)
{
    if (RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; apply_acl_roundtrip runs on Windows");
        return;
    }

    TempDir dir;
    const Sid self = CurrentTokenUser();
    const AclSpec spec{self, {
        {self, FILE_ALL_ACCESS, Inherit::ContainerAndObject},
        {wellknown::System(), FILE_ALL_ACCESS, Inherit::ContainerAndObject},
    }};

    ApplyAcl(dir.path, spec);

    const ObservedAcl acl = ReadAcl(dir.path);
    const std::optional<std::string> reason = CheckMatchesSpec(acl, spec);
    BOOST_CHECK_MESSAGE(!reason.has_value(), reason.value_or(""));
    BOOST_REQUIRE(acl.owner.has_value());
    BOOST_CHECK_EQUAL(FormatSid(*acl.owner), FormatSid(self));
}

BOOST_AUTO_TEST_CASE(owner_only_dacl_sets_explicit_owner)
{
    if (RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; owner_only_dacl_sets_explicit_owner runs on Windows");
        return;
    }

    TempDir dir;
    const fs::path file = dir.path / "owned";
    {
        fsbridge::ofstream stream(file);
        stream << "win_acl test\n";
    }

    util::ApplyOwnerOnlyDacl(file, /*inheritable=*/false);

    const ObservedAcl acl = ReadAcl(file);
    BOOST_REQUIRE(acl.owner.has_value());
    BOOST_CHECK_EQUAL(FormatSid(*acl.owner), FormatSid(CurrentTokenUser()));
    BOOST_CHECK_NO_THROW(util::VerifyOwnerOnlyDacl(file));
}

BOOST_AUTO_TEST_CASE(security_descriptor_creates_a_matching_file)
{
    if (RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; security_descriptor_creates_a_matching_file runs on Windows");
        return;
    }

    TempDir dir;
    const fs::path file = dir.path / "created";
    const Sid self = CurrentTokenUser();
    const AclSpec spec{self, {
        {self, FILE_ALL_ACCESS, Inherit::None},
        {wellknown::System(), FILE_ALL_ACCESS, Inherit::None},
    }};

    void* sd = MakeSecurityDescriptor(spec);
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;
    // READ_CONTROL, for reading the ACL back through the handle.
    const HANDLE handle = ::CreateFileW(file.wstring().c_str(), GENERIC_WRITE | READ_CONTROL, 0, &sa, CREATE_NEW,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
    FreeSecurityDescriptor(sd);
    BOOST_REQUIRE(handle != INVALID_HANDLE_VALUE);

    // The same ACL read through the handle and through the path.
    const ObservedAcl by_handle = ReadAcl(handle, /*kernel_object=*/false);
    ::CloseHandle(handle);
    const std::optional<std::string> handle_reason = CheckMatchesSpec(by_handle, spec);
    BOOST_CHECK_MESSAGE(!handle_reason.has_value(), handle_reason.value_or(""));

    const std::optional<std::string> path_reason = CheckMatchesSpec(ReadAcl(file), spec);
    BOOST_CHECK_MESSAGE(!path_reason.has_value(), path_reason.value_or(""));
}

BOOST_AUTO_TEST_CASE(foreign_writer_on_chain_needs_a_plain_absolute_path)
{
    const std::vector<Sid> allowed{wellknown::System(), wellknown::Administrators(), wellknown::TrustedInstaller()};

    BOOST_CHECK(FindForeignWriterOnChain(fs::path("relative") / "file.exe", allowed).has_value());
    BOOST_CHECK(FindForeignWriterOnChain(fs::path("C:\\Windows\\..\\file.exe"), allowed).has_value());
    BOOST_CHECK(FindForeignWriterOnChain(fs::path("C:\\Windows\\.\\file.exe"), allowed).has_value());
}

#endif // WIN32

BOOST_AUTO_TEST_SUITE_END()
