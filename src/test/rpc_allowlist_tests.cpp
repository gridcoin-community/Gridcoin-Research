// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <rpc/server.h>
#include <netbase.h>
#include <netaddress.h>

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

namespace {
//! Rebuild the allow list from a fresh set of -rpcallowip values.
void SetAllowList(const std::vector<std::string>& entries)
{
    InitRPCAllowList(entries);
}

bool Allowed(const std::string& addr)
{
    return ClientAllowed(boost::asio::ip::make_address(addr));
}
} // namespace

BOOST_AUTO_TEST_SUITE(rpc_allowlist_tests)

// A subnet in -rpcallowip used to match nothing at all: the entry was compared
// as text against the peer's address string, so an operator who wrote a CIDR
// prefix got a listener that refused every connection.
BOOST_AUTO_TEST_CASE(cidr_entries_match_their_subnet)
{
    SetAllowList({"10.9.8.0/24"});

    BOOST_CHECK(Allowed("10.9.8.1"));
    BOOST_CHECK(Allowed("10.9.8.255"));
    BOOST_CHECK(!Allowed("10.9.9.1"));
    BOOST_CHECK(!Allowed("11.9.8.1"));
}

// The address/netmask spelling has to work too, since both are in the field.
BOOST_AUTO_TEST_CASE(netmask_entries_match_their_subnet)
{
    SetAllowList({"172.16.0.0/255.255.0.0"});

    BOOST_CHECK(Allowed("172.16.5.9"));
    BOOST_CHECK(!Allowed("172.17.5.9"));
}

// A bare address is a host route, not a prefix.
BOOST_AUTO_TEST_CASE(bare_address_entries_match_exactly_one_host)
{
    SetAllowList({"203.0.113.7"});

    BOOST_CHECK(Allowed("203.0.113.7"));
    BOOST_CHECK(!Allowed("203.0.113.8"));
}

// The wildcard spelling predates subnet support and must keep working, or
// upgrading silently locks out operators who wrote it.
BOOST_AUTO_TEST_CASE(wildcard_entries_still_match)
{
    SetAllowList({"192.168.44.*"});

    BOOST_CHECK(Allowed("192.168.44.3"));
    BOOST_CHECK(!Allowed("192.168.45.3"));
}

BOOST_AUTO_TEST_CASE(ipv6_subnets_match)
{
    SetAllowList({"2001:470::/32"});

    BOOST_CHECK(Allowed("2001:470::1"));
    BOOST_CHECK(Allowed("2001:470:ffff::9"));
    BOOST_CHECK(!Allowed("2001:471::1"));
}

// Addresses CNetAddr rejects outright can never be allowed, whatever the list
// says. 2001:db8::/32 is the RFC 3849 documentation prefix, which IsValid()
// refuses, so Match() declines it even against a rule naming that exact
// subnet. Pinned because it is surprising: the rule parses, logs as accepted,
// and still matches nothing.
BOOST_AUTO_TEST_CASE(documentation_addresses_are_never_allowed)
{
    SetAllowList({"2001:db8::/32"});

    BOOST_CHECK(!Allowed("2001:db8::1"));
}

// Loopback bypasses the list entirely -- including when the list is empty,
// which is the default configuration for every node.
BOOST_AUTO_TEST_CASE(loopback_is_always_allowed)
{
    SetAllowList({});

    BOOST_CHECK(Allowed("127.0.0.1"));
    BOOST_CHECK(Allowed("127.0.0.5"));
    BOOST_CHECK(Allowed("::1"));
    BOOST_CHECK(!Allowed("10.9.8.1"));
}

// An IPv4-mapped v6 peer must be judged by its v4 address, or a v4 rule
// silently fails to cover the same host arriving over a dual-stack socket --
// which is exactly what the default wildcard bind produces.
BOOST_AUTO_TEST_CASE(v4_mapped_v6_is_matched_as_v4)
{
    SetAllowList({"10.9.8.0/24"});

    BOOST_CHECK(Allowed("::ffff:10.9.8.1"));
    BOOST_CHECK(!Allowed("::ffff:10.9.9.1"));
}

// Several entries, mixed spellings, all live at once.
BOOST_AUTO_TEST_CASE(entries_are_independent)
{
    SetAllowList({"10.9.8.0/24", "192.168.44.*", "203.0.113.7"});

    BOOST_CHECK(Allowed("10.9.8.200"));
    BOOST_CHECK(Allowed("192.168.44.1"));
    BOOST_CHECK(Allowed("203.0.113.7"));
    BOOST_CHECK(!Allowed("8.8.8.8"));
}

// An unparseable entry must not take the rest of the list down with it.
// A link-local peer stringifies with its zone appended ("fe80::1%2"), which is
// not a form an operator can put in an entry: the zone is a local interface
// index, not a property of the peer. Matching has to survive it.
//
// It does, and without the allow list doing anything about it: LookupHost()
// resolves through getaddrinfo(), which parses the zone and reports it
// separately (see the sin6_scope_id read in netbase.cpp), so the conversion
// succeeds and the subnet comparison runs on the address alone. Pinned here
// because that is a property of a dependency rather than of this file, and
// nothing else would notice if it changed.
BOOST_AUTO_TEST_CASE(link_local_scope_suffix_does_not_defeat_matching)
{
    SetAllowList({"fe80::/10"});

    BOOST_CHECK(Allowed("fe80::1"));

    // The same address, carrying a scope id.
    boost::system::error_code ec;
    const auto scoped = boost::asio::ip::make_address("fe80::1%2", ec);
    BOOST_REQUIRE(!ec);

    // Guard the premise: if this ever stops carrying the zone, the case below
    // is testing nothing.
    BOOST_REQUIRE(scoped.to_string().find('%') != std::string::npos);

    BOOST_CHECK(ClientAllowed(scoped));

    // Still outside the prefix, zone or no zone.
    BOOST_CHECK(!Allowed("fec0::1"));
}

BOOST_AUTO_TEST_CASE(unparseable_entries_do_not_disable_the_others)
{
    SetAllowList({"this-is-not-an-address", "10.9.8.0/24"});

    BOOST_CHECK(Allowed("10.9.8.1"));
    BOOST_CHECK(!Allowed("8.8.8.8"));
}

BOOST_AUTO_TEST_SUITE_END()
