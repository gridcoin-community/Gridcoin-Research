// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "util/win_acl.h"

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifdef WIN32
#include "test/wine.h"
#include "util/dir_permissions.h"

#include <windows.h>
#include <sddl.h>
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

using Bytes = std::vector<unsigned char>;

void AppendLittleEndian(Bytes& bytes, uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<unsigned char>(value >> shift));
    }
}

//! Write \p size into the entry's size field.
void SetAceSize(Bytes& bytes, size_t size)
{
    bytes[2] = static_cast<unsigned char>(size);
    bytes[3] = static_cast<unsigned char>(size >> 8);
}

//! An entry as Windows lays it out: the type, the flags and the 16-bit little-endian
//! size, then the mask, \p body (object flags and GUIDs, or a compound type), the
//! trustee's SID and \p tail.
Bytes AceBytes(uint8_t type, uint8_t flags, uint32_t access, const Bytes& body, const Sid& trustee,
               const Bytes& tail = {})
{
    Bytes bytes{type, flags, 0, 0};
    AppendLittleEndian(bytes, access);
    bytes.insert(bytes.end(), body.begin(), body.end());
    bytes.insert(bytes.end(), trustee.begin(), trustee.end());
    bytes.insert(bytes.end(), tail.begin(), tail.end());

    SetAceSize(bytes, bytes.size());

    return bytes;
}

//! The flags field of an object entry, then a GUID for each flag set.
Bytes ObjectBody(uint32_t object_flags, const Bytes& guid)
{
    Bytes body;
    AppendLittleEndian(body, object_flags);
    if (object_flags & 0x1) body.insert(body.end(), guid.begin(), guid.end());
    if (object_flags & 0x2) body.insert(body.end(), guid.begin(), guid.end());

    return body;
}

std::optional<ObservedAce> Parse(const Bytes& bytes)
{
    return ParseAce(bytes.data(), bytes.size());
}

//! One entry of each allow and deny type, with the kind it is read as.
struct AceCase
{
    uint8_t type{0};
    Bytes body;
    Bytes tail;
    AceKind kind{AceKind::Other};
};

std::vector<AceCase> AllowAndDenyCases()
{
    // A compound entry carries a 16-bit compound type and 16 reserved bits before its
    // first SID; a callback entry carries application data after its SID.
    const Bytes compound{0x01, 0x00, 0x00, 0x00};
    const Bytes application_data{0xDE, 0xAD, 0xBE, 0xEF};

    return {
        {0x00, {}, {}, AceKind::Allow},
        {0x01, {}, {}, AceKind::Deny},
        {0x04, compound, {}, AceKind::OtherAllow},
        {0x05, ObjectBody(0, {}), {}, AceKind::OtherAllow},
        {0x06, ObjectBody(0, {}), {}, AceKind::Deny},
        {0x09, {}, application_data, AceKind::OtherAllow},
        {0x0A, {}, application_data, AceKind::Deny},
        {0x0B, ObjectBody(0, {}), application_data, AceKind::OtherAllow},
        {0x0C, ObjectBody(0, {}), application_data, AceKind::Deny},
    };
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

BOOST_AUTO_TEST_CASE(sid_string_must_be_canonical)
{
    // Each SID has one string form, the one FormatSid writes: no leading zeros, an
    // authority below 2^32 in decimal, and one of 2^32 or more as "0x" and twelve
    // upper-case hex digits.
    for (const char* text : {"S-1-5-018", "S-1-05-18", "S-1-5-00", "S-1-0x5-18", "S-1-4294967296-7",
                             "S-1-0x0000FFFFFFFF-7", "S-1-0x123456789abc-7", "S-1-0X123456789ABC-7",
                             "S-1-0x23456789ABC-7", "S-1-0x0123456789ABC-7", "S-1-0x-7"}) {
        BOOST_CHECK_MESSAGE(!ParseSidString(text).has_value(), "'" << text << "' parses");
    }

    // Zero itself is canonical, as in the Null SID.
    for (const char* text : {"S-1-0-0", "S-1-5-0", "S-1-0x000100000000-7", "S-1-4294967295-7"}) {
        const std::optional<Sid> sid = ParseSidString(text);
        BOOST_REQUIRE_MESSAGE(sid.has_value(), "'" << text << "' does not parse");
        BOOST_CHECK_EQUAL(FormatSid(*sid), text);
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
    const Sid wide{0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00};
    BOOST_CHECK_EQUAL(FormatSid(wide), "S-1-0x000100000000-7");
    BOOST_CHECK(ParseSidString("S-1-0x000100000000-7") == wide);
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

BOOST_AUTO_TEST_CASE(parse_ace_reads_every_allow_and_deny_type)
{
    for (const AceCase& c : AllowAndDenyCases()) {
        const std::optional<ObservedAce> ace = Parse(AceBytes(c.type, 0x00, 0x0012019F, c.body, wellknown::Users(), c.tail));
        BOOST_REQUIRE_MESSAGE(ace.has_value(), "type " << int{c.type});
        BOOST_CHECK_MESSAGE(ace->kind == c.kind, "type " << int{c.type});
        BOOST_CHECK_MESSAGE(ace->trustee == wellknown::Users(), "type " << int{c.type} << ": " << FormatSid(ace->trustee));
        BOOST_CHECK_EQUAL(ace->mask, 0x0012019Fu);
        BOOST_CHECK(ace->inherit == Inherit::None);
        BOOST_CHECK(!ace->inherit_only);
        BOOST_CHECK(!ace->inherited);
    }
}

BOOST_AUTO_TEST_CASE(parse_ace_reads_audit_and_label_types_as_other)
{
    // A system audit entry, and a mandatory label of High integrity that forbids writes up.
    const std::optional<ObservedAce> audit = Parse(AceBytes(0x02, 0x40, FILE_WRITE_DATA, {}, wellknown::Users()));
    BOOST_REQUIRE(audit.has_value());
    BOOST_CHECK(audit->kind == AceKind::Other);
    BOOST_CHECK(audit->trustee == wellknown::Users());
    BOOST_CHECK_EQUAL(audit->mask, static_cast<uint32_t>(FILE_WRITE_DATA));

    const std::optional<ObservedAce> label = Parse(AceBytes(0x11, 0x00, 0x1, {}, SidOf("S-1-16-12288")));
    BOOST_REQUIRE(label.has_value());
    BOOST_CHECK(label->kind == AceKind::Other);
    BOOST_CHECK_EQUAL(FormatSid(label->trustee), "S-1-16-12288");
    BOOST_CHECK_EQUAL(label->mask, 0x1u);

    // A type with no known layout is read as its header alone.
    const std::optional<ObservedAce> unknown = Parse(Bytes{0x30, 0x00, 0x04, 0x00});
    BOOST_REQUIRE(unknown.has_value());
    BOOST_CHECK(unknown->kind == AceKind::Other);
    BOOST_CHECK(unknown->trustee.empty());
    BOOST_CHECK_EQUAL(unknown->mask, 0u);
}

BOOST_AUTO_TEST_CASE(parse_ace_finds_the_sid_after_object_guids)
{
    // Each GUID holds the bytes of the Users SID, so reading the trustee from too early
    // an offset finds Users rather than Administrators.
    const Bytes guid = wellknown::Users();
    BOOST_REQUIRE_EQUAL(guid.size(), 16u);

    for (const uint8_t type : {uint8_t{0x05}, uint8_t{0x06}, uint8_t{0x0B}, uint8_t{0x0C}}) {
        for (const uint32_t object_flags : {0u, 1u, 2u, 3u}) {
            const Bytes bytes = AceBytes(type, 0x00, FILE_ALL_ACCESS, ObjectBody(object_flags, guid),
                                         wellknown::Administrators());
            const std::optional<ObservedAce> ace = Parse(bytes);
            BOOST_REQUIRE_MESSAGE(ace.has_value(), "type " << int{type} << ", object flags " << object_flags);
            BOOST_CHECK_MESSAGE(ace->trustee == wellknown::Administrators(),
                                "type " << int{type} << ", object flags " << object_flags << ": "
                                        << FormatSid(ace->trustee));
            BOOST_CHECK_EQUAL(ace->mask, static_cast<uint32_t>(FILE_ALL_ACCESS));
        }
    }
}

BOOST_AUTO_TEST_CASE(parse_ace_maps_inheritance_flags)
{
    struct FlagsCase
    {
        uint8_t flags{0};
        std::optional<Inherit> inherit;
        bool inherit_only{false};
        bool inherited{false};
    };

    // Object inherit 0x1, container inherit 0x2, no-propagate 0x4, inherit-only 0x8,
    // inherited 0x10.
    const std::vector<FlagsCase> cases{
        {0x00, Inherit::None, false, false},
        {0x03, Inherit::ContainerAndObject, false, false},
        {0x01, std::nullopt, false, false},
        {0x02, std::nullopt, false, false},
        {0x07, std::nullopt, false, false},
        {0x0B, Inherit::ContainerAndObject, true, false},
        {0x10, Inherit::None, false, true},
        {0x13, Inherit::ContainerAndObject, false, true},
    };

    for (const FlagsCase& c : cases) {
        const std::optional<ObservedAce> ace = Parse(AceBytes(0x00, c.flags, FILE_ALL_ACCESS, {}, wellknown::Users()));
        BOOST_REQUIRE(ace.has_value());
        BOOST_CHECK_MESSAGE(ace->inherit == c.inherit, "flags " << int{c.flags});
        BOOST_CHECK_MESSAGE(ace->inherit_only == c.inherit_only, "flags " << int{c.flags});
        BOOST_CHECK_MESSAGE(ace->inherited == c.inherited, "flags " << int{c.flags});
    }
}

BOOST_AUTO_TEST_CASE(parse_ace_refuses_truncated_entries)
{
    BOOST_CHECK(!ParseAce(nullptr, 16).has_value());

    // Shorter than the header, or a size field below the header's own size.
    for (const Bytes& bytes : {Bytes{}, Bytes{0x00}, Bytes{0x00, 0x00, 0x08}, Bytes{0x00, 0x00, 0x03, 0x00}}) {
        BOOST_CHECK_MESSAGE(!Parse(bytes).has_value(), bytes.size() << " bytes");
    }

    for (const AceCase& c : AllowAndDenyCases()) {
        const Bytes whole = AceBytes(c.type, 0x00, FILE_ALL_ACCESS, c.body, wellknown::Users());
        BOOST_REQUIRE(Parse(whole).has_value());

        // Cut short, whether the size field still claims the whole entry or is cut with it.
        // Each copy is exactly as long as what may be read.
        for (size_t length = 0; length < whole.size(); ++length) {
            Bytes claims_whole(whole.begin(), whole.begin() + length);
            BOOST_CHECK_MESSAGE(!Parse(claims_whole).has_value(), "type " << int{c.type} << ", " << length << " bytes");

            if (length >= 4) {
                Bytes cut = claims_whole;
                SetAceSize(cut, length);
                BOOST_CHECK_MESSAGE(!Parse(cut).has_value(),
                                    "type " << int{c.type} << ", size field " << length);
            }
        }
    }

    // A size field that ends inside the SID, with the rest of the SID readable after it.
    Bytes ends_early = AceBytes(0x00, 0x00, FILE_ALL_ACCESS, {}, wellknown::Users());
    SetAceSize(ends_early, 12);
    BOOST_CHECK(!Parse(ends_early).has_value());

    // A SID that claims more sub-authorities than the entry holds.
    Bytes long_count = AceBytes(0x00, 0x00, FILE_ALL_ACCESS, {}, wellknown::Users());
    long_count[9] = 5;
    BOOST_CHECK(!Parse(long_count).has_value());

    // Object flags that call for a GUID the entry does not hold.
    Bytes missing_guid = AceBytes(0x05, 0x00, FILE_ALL_ACCESS, ObjectBody(0, {}), wellknown::Users());
    missing_guid[8] = 0x03;
    BOOST_CHECK(!Parse(missing_guid).has_value());

    // A SID that is not revision 1.
    Bytes revision = AceBytes(0x00, 0x00, FILE_ALL_ACCESS, {}, wellknown::Users());
    revision[8] = 2;
    BOOST_CHECK(!Parse(revision).has_value());
}

namespace {

//! The handles RecordingCloser was asked to close, in order.
std::vector<void*> g_closed;

struct RecordingCloser
{
    void operator()(void* handle) const noexcept { g_closed.push_back(handle); }
};

using RecordedHandle = BasicUniqueHandle<RecordingCloser>;

//! A handle value of \p n, never dereferenced.
void* FakeHandle(std::intptr_t n)
{
    return reinterpret_cast<void*>(n);
}

} // namespace

// Null and INVALID_HANDLE_VALUE are no handle and are never closed. Any other is
// closed once: when its guard goes, or is reset, and never after it was released or
// moved to another guard.
BOOST_AUTO_TEST_CASE(unique_handle_closes_each_handle_once)
{
    g_closed.clear();
    {
        const RecordedHandle none;
        const RecordedHandle null(nullptr);
        const RecordedHandle invalid(FakeHandle(-1));
        BOOST_CHECK(!none && !null && !invalid);
        BOOST_CHECK(invalid.get() == nullptr);
    }
    BOOST_CHECK(g_closed.empty());

    {
        const RecordedHandle a(FakeHandle(0x10));
        BOOST_CHECK(static_cast<bool>(a));
        BOOST_CHECK(a.get() == FakeHandle(0x10));
    }
    BOOST_REQUIRE_EQUAL(g_closed.size(), 1U);
    BOOST_CHECK(g_closed[0] == FakeHandle(0x10));

    g_closed.clear();
    {
        RecordedHandle a(FakeHandle(0x20));
        a.reset(FakeHandle(0x30));
        BOOST_REQUIRE_EQUAL(g_closed.size(), 1U);
        BOOST_CHECK(g_closed[0] == FakeHandle(0x20));
        a.reset(FakeHandle(-1));
        BOOST_REQUIRE_EQUAL(g_closed.size(), 2U);
        BOOST_CHECK(g_closed[1] == FakeHandle(0x30));
        BOOST_CHECK(!a);
    }
    BOOST_CHECK_EQUAL(g_closed.size(), 2U);

    g_closed.clear();
    {
        RecordedHandle a(FakeHandle(0x40));
        BOOST_CHECK(a.release() == FakeHandle(0x40));
        BOOST_CHECK(!a);
    }
    BOOST_CHECK(g_closed.empty());

    {
        RecordedHandle a(FakeHandle(0x50));
        RecordedHandle b(std::move(a));
        BOOST_CHECK(!a);
        BOOST_CHECK(b.get() == FakeHandle(0x50));
        RecordedHandle c(FakeHandle(0x60));
        c = std::move(b);
        BOOST_REQUIRE_EQUAL(g_closed.size(), 1U);
        BOOST_CHECK(g_closed[0] == FakeHandle(0x60));
        BOOST_CHECK(c.get() == FakeHandle(0x50));
    }
    BOOST_REQUIRE_EQUAL(g_closed.size(), 2U);
    BOOST_CHECK(g_closed[1] == FakeHandle(0x50));
    g_closed.clear();
}

#ifdef WIN32

using grc_test::RunningUnderWine;

namespace {

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

//! Create the file \p path with \p spec as its owner and DACL from the start.
void CreateWithAcl(const fs::path& path, const AclSpec& spec)
{
    void* sd = MakeSecurityDescriptor(spec);
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = sd;
    sa.bInheritHandle = FALSE;
    const HANDLE handle = ::CreateFileW(path.wstring().c_str(), GENERIC_WRITE, 0, &sa, CREATE_NEW,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
    const DWORD error = ::GetLastError();
    FreeSecurityDescriptor(sd);

    BOOST_REQUIRE_MESSAGE(handle != INVALID_HANDLE_VALUE, "CreateFileW failed with error " << error);
    ::CloseHandle(handle);
}

//! The owner this process's token gives what it creates when no descriptor names one.
Sid DefaultOwner()
{
    HANDLE token = nullptr;
    BOOST_REQUIRE(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token));

    // The first call sizes the buffer; it fails with ERROR_INSUFFICIENT_BUFFER by design.
    DWORD length = 0;
    ::GetTokenInformation(token, TokenOwner, nullptr, 0, &length);
    std::vector<unsigned char> buffer(length ? length : 1);
    const bool queried = ::GetTokenInformation(token, TokenOwner, buffer.data(), length, &length);
    ::CloseHandle(token);
    BOOST_REQUIRE(queried);

    const PSID owner = reinterpret_cast<const TOKEN_OWNER*>(buffer.data())->Owner;
    const auto* const bytes = static_cast<const unsigned char*>(owner);

    return Sid(bytes, bytes + ::GetLengthSid(owner));
}

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

// A directory created with its list has exactly that list, owner included, and an
// existing one is left alone.
BOOST_AUTO_TEST_CASE(create_directory_with_acl_reads_back_and_leaves_an_existing_one)
{
    if (RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; create_directory_with_acl runs on Windows");
        return;
    }

    TempDir dir;
    const Sid self = CurrentTokenUser();
    const AclSpec spec{self, {
        {self, FILE_ALL_ACCESS, Inherit::ContainerAndObject},
        {wellknown::System(), FILE_ALL_ACCESS, Inherit::ContainerAndObject},
    }};

    const fs::path made = dir.path / "made";
    BOOST_CHECK(CreateDirectoryWithAcl(made, spec));
    const ObservedAcl acl = ReadAcl(made);
    const std::optional<std::string> reason = CheckMatchesSpec(acl, spec);
    BOOST_CHECK_MESSAGE(!reason.has_value(), reason.value_or(""));

    // The existing temp directory keeps the list it inherited.
    const ObservedAcl before = ReadAcl(dir.path);
    BOOST_CHECK(!CreateDirectoryWithAcl(dir.path, spec));
    BOOST_CHECK(!CreateDirectoryWithAcl(made, spec));
    const ObservedAcl after = ReadAcl(dir.path);
    BOOST_CHECK_EQUAL(after.aces.size(), before.aces.size());
    BOOST_CHECK_EQUAL(after.is_protected, before.is_protected);

    // A parent that does not exist is an error, not "already there".
    BOOST_CHECK_THROW(CreateDirectoryWithAcl(dir.path / "absent" / "made", spec), std::system_error);
}

BOOST_AUTO_TEST_CASE(owner_only_dacl_sets_explicit_owner)
{
    if (RunningUnderWine()) {
        BOOST_TEST_MESSAGE("Wine keeps no protected DACL; owner_only_dacl_sets_explicit_owner runs on Windows");
        return;
    }

    TempDir dir;
    const fs::path file = dir.path / "owned";
    const Sid self = CurrentTokenUser();

    if (!CurrentTokenIsElevated()) {
        // This account owns the file and may do anything to it but change its owner.
        // An owner that is already right is left alone, so restricting it needs no
        // WRITE_OWNER.
        CreateWithAcl(file, {self, {
            {self, FILE_ALL_ACCESS & ~WRITE_OWNER, Inherit::None},
            {wellknown::System(), FILE_ALL_ACCESS, Inherit::None},
        }});
        BOOST_REQUIRE(ReadAcl(file).owner == self);

        BOOST_REQUIRE_NO_THROW(util::ApplyOwnerOnlyDacl(file, /*inheritable=*/false));
        BOOST_CHECK(ReadAcl(file).owner == self);
        BOOST_CHECK_NO_THROW(util::VerifyOwnerOnlyDacl(file));

        BOOST_TEST_MESSAGE("owner_only_dacl_sets_explicit_owner: not elevated, so the change of owner from "
                           "Administrators did not run; run the suite elevated to cover it");
        return;
    }

    // Only an elevated token may make Administrators a file's owner. The owner-only ACL
    // has to replace that owner with this account.
    CreateWithAcl(file, {wellknown::Administrators(), {
        {self, FILE_ALL_ACCESS, Inherit::None},
        {wellknown::System(), FILE_ALL_ACCESS, Inherit::None},
    }});
    BOOST_REQUIRE(ReadAcl(file).owner == wellknown::Administrators());

    util::ApplyOwnerOnlyDacl(file, /*inheritable=*/false);

    const ObservedAcl acl = ReadAcl(file);
    BOOST_REQUIRE(acl.owner.has_value());
    BOOST_CHECK_EQUAL(FormatSid(*acl.owner), FormatSid(self));
    BOOST_CHECK_NO_THROW(util::VerifyOwnerOnlyDacl(file));

    BOOST_TEST_MESSAGE("owner_only_dacl_sets_explicit_owner: elevated, so keeping an owner that is already right "
                       "without WRITE_OWNER did not run; run the suite unelevated to cover it");
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

    // The owner checks above tell the descriptor's owner apart from the creating
    // token's default owner only when the two differ, as for an elevated token, whose
    // default owner is normally the Administrators group.
    if (DefaultOwner() == self) {
        BOOST_TEST_MESSAGE("security_descriptor_creates_a_matching_file: this token's default owner is this "
                           "account, so the owner check cannot fail; run the suite elevated to cover it");
    }
}

BOOST_AUTO_TEST_CASE(foreign_writer_on_chain_needs_a_plain_absolute_path)
{
    const std::vector<Sid> allowed{wellknown::System(), wellknown::Administrators(), wellknown::TrustedInstaller()};

    BOOST_CHECK(FindForeignWriterOnChain(fs::path("relative") / "file.exe", allowed).has_value());
    BOOST_CHECK(FindForeignWriterOnChain(fs::path("C:\\Windows\\..\\file.exe"), allowed).has_value());
    BOOST_CHECK(FindForeignWriterOnChain(fs::path("C:\\Windows\\.\\file.exe"), allowed).has_value());
}

BOOST_AUTO_TEST_CASE(parse_ace_reads_entries_windows_builds)
{
    if (RunningUnderWine()) {
        BOOST_TEST_MESSAGE("parse_ace_reads_entries_windows_builds reads Windows' own SDDL conversion; it runs on Windows");
        return;
    }

    // A plain allow, a conditional (callback) allow, an object allow with an object type,
    // and an object deny with an inherited object type. FA is FILE_ALL_ACCESS and CC is
    // the directory service right to create a child (0x1).
    const wchar_t* const sddl = L"D:P(A;;FA;;;SY)(XA;;FA;;;WD;(Member_of {SID(BA)}))"
                                L"(OA;;CC;bf967aba-0de6-11d0-a285-00aa003049e2;;WD)"
                                L"(OD;;CC;;bf967aba-0de6-11d0-a285-00aa003049e2;WD)";
    PSECURITY_DESCRIPTOR sd = nullptr;
    BOOST_REQUIRE(::ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd, nullptr));

    BOOL present = FALSE;
    BOOL defaulted = FALSE;
    PACL dacl = nullptr;
    const bool have_dacl = ::GetSecurityDescriptorDacl(sd, &present, &dacl, &defaulted) && present && dacl != nullptr;

    std::vector<std::optional<ObservedAce>> aces;
    if (have_dacl) {
        const auto* const end = reinterpret_cast<const unsigned char*>(dacl) + dacl->AclSize;
        for (DWORD i = 0; i < dacl->AceCount; ++i) {
            void* entry = nullptr;
            if (!::GetAce(dacl, i, &entry)) break;
            const auto* const bytes = static_cast<const unsigned char*>(entry);
            aces.push_back(ParseAce(bytes, static_cast<size_t>(end - bytes)));
        }
    }
    ::LocalFree(sd);

    BOOST_REQUIRE(have_dacl);
    BOOST_REQUIRE_EQUAL(aces.size(), 4u);
    for (const std::optional<ObservedAce>& ace : aces) {
        BOOST_REQUIRE(ace.has_value());
    }

    const Sid everyone = SidOf("S-1-1-0");
    BOOST_CHECK(aces[0]->kind == AceKind::Allow);
    BOOST_CHECK(aces[0]->trustee == wellknown::System());
    BOOST_CHECK_EQUAL(aces[0]->mask, static_cast<uint32_t>(FILE_ALL_ACCESS));
    BOOST_CHECK(aces[1]->kind == AceKind::OtherAllow);
    BOOST_CHECK(aces[1]->trustee == everyone);
    BOOST_CHECK_EQUAL(aces[1]->mask, static_cast<uint32_t>(FILE_ALL_ACCESS));
    BOOST_CHECK(aces[2]->kind == AceKind::OtherAllow);
    BOOST_CHECK(aces[2]->trustee == everyone);
    BOOST_CHECK_EQUAL(aces[2]->mask, 0x1u);
    BOOST_CHECK(aces[3]->kind == AceKind::Deny);
    BOOST_CHECK(aces[3]->trustee == everyone);
    BOOST_CHECK_EQUAL(aces[3]->mask, 0x1u);
}

#endif // WIN32

BOOST_AUTO_TEST_SUITE_END()
