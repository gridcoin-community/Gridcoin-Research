#include <boost/assign/list_of.hpp>
#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>
#include "util.h"

#include "clientversion.h"
#include "netbase.h"
#include "net.h"
#include "protocol.h"
#include "serialize.h"
#include "span.h"
#include "streams.h"
#include "version.h"
#include <ios>
#include "util/strencodings.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(netbase_tests)

static CNetAddr ResolveIP(const char* ip)
{
    CNetAddr addr;
    LookupHost(ip, addr, false);
    return addr;
}

static CSubNet ResolveSubNet(const char* subnet)
{
    CSubNet ret;
    LookupSubNet(subnet, ret);
    return ret;
}


static CNetAddr CreateInternal(const char* host)
{
    CNetAddr addr;
    addr.SetInternal(host);
    return addr;
}

BOOST_AUTO_TEST_CASE(netbase_networks)
{
    BOOST_CHECK(ResolveIP("127.0.0.1").GetNetwork()                              == NET_UNROUTABLE);
    BOOST_CHECK(ResolveIP("::1").GetNetwork()                                    == NET_UNROUTABLE);
    BOOST_CHECK(ResolveIP("8.8.8.8").GetNetwork()                                == NET_IPV4);
    BOOST_CHECK(ResolveIP("2001::8888").GetNetwork()                             == NET_IPV6);
    BOOST_CHECK(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetNetwork() == NET_TOR);
    BOOST_CHECK(CreateInternal("foo.com").GetNetwork()                           == NET_INTERNAL);
}

BOOST_AUTO_TEST_CASE(netbase_lookup_numeric)
{
    // Run with and without configured IPv6: allowing DNS must not prevent
    // numeric literals from being decoded by LookupHost or Lookup.
    for (const char* ip : {"127.0.0.1", "2001:db8::1", "fc00:1:2:3:4:5:6:7"}) {
        const CNetAddr expected = ResolveIP(ip);
        for (const bool allow_lookup : {false, true}) {
            CNetAddr addr;
            BOOST_REQUIRE(LookupHost(ip, addr, allow_lookup));
            BOOST_CHECK(addr == expected);
            CService service;
            BOOST_REQUIRE(Lookup(ip, service, 32749, allow_lookup));
            BOOST_CHECK(static_cast<const CNetAddr&>(service) == expected);
            BOOST_CHECK_EQUAL(service.GetPort(), 32749);
        }
    }

    CNetAddr addr;
    BOOST_CHECK(!LookupHost("localhost", addr, false));
    BOOST_REQUIRE(LookupHost("localhost", addr, true));
    BOOST_CHECK(addr.IsLocal());
}

BOOST_AUTO_TEST_CASE(netbase_properties)
{
    BOOST_CHECK(ResolveIP("127.0.0.1").IsIPv4());
    BOOST_CHECK(ResolveIP("::FFFF:192.168.1.1").IsIPv4());
    BOOST_CHECK(ResolveIP("::1").IsIPv6());
    BOOST_CHECK(ResolveIP("10.0.0.1").IsRFC1918());
    BOOST_CHECK(ResolveIP("192.168.1.1").IsRFC1918());
    BOOST_CHECK(ResolveIP("172.31.255.255").IsRFC1918());
    BOOST_CHECK(ResolveIP("2001:0DB8::").IsRFC3849());
    BOOST_CHECK(ResolveIP("169.254.1.1").IsRFC3927());
    BOOST_CHECK(ResolveIP("2002::1").IsRFC3964());
    BOOST_CHECK(ResolveIP("FC00::").IsRFC4193());
    BOOST_CHECK(ResolveIP("2001::2").IsRFC4380());
    BOOST_CHECK(ResolveIP("2001:10::").IsRFC4843());
    BOOST_CHECK(ResolveIP("FE80::").IsRFC4862());
    BOOST_CHECK(ResolveIP("64:FF9B::").IsRFC6052());
    BOOST_CHECK(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").IsTor());
    BOOST_CHECK(ResolveIP("127.0.0.1").IsLocal());
    BOOST_CHECK(ResolveIP("::1").IsLocal());
    BOOST_CHECK(ResolveIP("8.8.8.8").IsRoutable());
    BOOST_CHECK(ResolveIP("2001::1").IsRoutable());
    BOOST_CHECK(ResolveIP("127.0.0.1").IsValid());
    BOOST_CHECK(CreateInternal("FD6B:88C0:8724:edb1:8e4:3588:e546:35ca").IsInternal());
    BOOST_CHECK(CreateInternal("bar.com").IsInternal());
}

bool static TestSplitHost(string test, string host, int port)
{
    string hostOut;
    int portOut = -1;
    SplitHostPort(test, portOut, hostOut);
    return hostOut == host && port == portOut;
}

BOOST_AUTO_TEST_CASE(netbase_parsenetwork_cjdns)
{
    BOOST_CHECK_EQUAL(ParseNetwork("cjdns"), NET_CJDNS);
    BOOST_CHECK_EQUAL(ParseNetwork("CJDNS"), NET_CJDNS);
    BOOST_CHECK_EQUAL(ParseNetwork("tor"), NET_TOR);
}

BOOST_AUTO_TEST_CASE(netbase_cjdns_getgroup)
{
    // bitcoin#23077: skip the constant 0xfc byte, take 4 random bits after it.
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
    s.write(MakeByteSpan(ParseHex("0610fc000001000200030004000500060007")));
    CNetAddr addr;
    s >> addr;
    BOOST_REQUIRE(addr.IsCJDNS());
    BOOST_CHECK(addr.GetGroup() == std::vector<unsigned char>({(unsigned char)NET_CJDNS, 0xfc, 0x0f}));
    BOOST_CHECK_EQUAL(addr.GetNetwork(), NET_CJDNS);
    BOOST_CHECK(addr.IsRoutable());
    CNetAddr copied;
    copied.SetIP(addr);
    BOOST_CHECK(copied == addr);
    struct in6_addr raw;
    BOOST_CHECK(addr.GetIn6Addr(&raw));
    CService service(addr, 32749);
    struct sockaddr_storage socket_addr;
    socklen_t length = sizeof(socket_addr);
    BOOST_REQUIRE(service.GetSockAddr(reinterpret_cast<struct sockaddr*>(&socket_addr), &length));
    BOOST_CHECK_EQUAL(socket_addr.ss_family, AF_INET6);
    BOOST_CHECK_EQUAL(service.ToStringIPPort(), "[fc00:1:2:3:4:5:6:7]:32749");
    CDataStream v1(SER_NETWORK, PROTOCOL_VERSION);
    v1 << addr;
    BOOST_CHECK_EQUAL(HexStr(v1), std::string(32, '0'));
    CDataStream v2(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
    v2 << addr;
    BOOST_CHECK_EQUAL(HexStr(v2), "0610fc000001000200030004000500060007");
}

BOOST_AUTO_TEST_CASE(netbase_cjdns_classification)
{
    const bool was_limited = IsLimited(NET_CJDNS);
    const CService fc = LookupNumeric("fc00:1:2:3:4:5:6:7", 32749);
    BOOST_CHECK(fc.HasCJDNSPrefix());
    SetReachable(NET_CJDNS, false);
    const CService disabled = MaybeFlipIPv6toCJDNS(fc);
    BOOST_CHECK(disabled.IsIPv6());
    BOOST_CHECK(!disabled.IsCJDNS());
    BOOST_CHECK(!disabled.IsRoutable());
    SetReachable(NET_CJDNS, true);
    const CService enabled = MaybeFlipIPv6toCJDNS(fc);
    BOOST_CHECK(enabled.IsCJDNS());
    BOOST_CHECK(enabled.IsRoutable());
    BOOST_CHECK_EQUAL(enabled.GetPort(), 32749);
    BOOST_CHECK_EQUAL(enabled.GetReachabilityFrom(&enabled), 6); // REACH_PRIVATE
    const CService other_ula = LookupNumeric("fd00::1", 32749);
    BOOST_CHECK(!other_ula.HasCJDNSPrefix());
    BOOST_CHECK(MaybeFlipIPv6toCJDNS(other_ula).IsIPv6());
    SetReachable(NET_CJDNS, !was_limited);
}

BOOST_AUTO_TEST_CASE(netbase_splithost)
{
    BOOST_CHECK(TestSplitHost("www.bitcoin.org", "www.bitcoin.org", -1));
    BOOST_CHECK(TestSplitHost("[www.bitcoin.org]", "www.bitcoin.org", -1));
    BOOST_CHECK(TestSplitHost("www.bitcoin.org:80", "www.bitcoin.org", 80));
    BOOST_CHECK(TestSplitHost("[www.bitcoin.org]:80", "www.bitcoin.org", 80));
    BOOST_CHECK(TestSplitHost("127.0.0.1", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("127.0.0.1:8333", "127.0.0.1", 8333));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]:8333", "127.0.0.1", 8333));
    BOOST_CHECK(TestSplitHost("::ffff:127.0.0.1", "::ffff:127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("[::ffff:127.0.0.1]:8333", "::ffff:127.0.0.1", 8333));
    BOOST_CHECK(TestSplitHost("[::]:8333", "::", 8333));
    BOOST_CHECK(TestSplitHost("::8333", "::8333", -1));
    BOOST_CHECK(TestSplitHost(":8333", "", 8333));
    BOOST_CHECK(TestSplitHost("[]:8333", "", 8333));
    BOOST_CHECK(TestSplitHost("", "", -1));
}

bool static TestParse(string src, string canon)
{
    CService addr(LookupNumeric(src.c_str(), 65535));
    return canon == addr.ToString();
}

BOOST_AUTO_TEST_CASE(netbase_lookupnumeric)
{
    BOOST_CHECK(TestParse("127.0.0.1", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse("127.0.0.1:8333", "127.0.0.1:8333"));
    BOOST_CHECK(TestParse("::ffff:127.0.0.1", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse("::", "[::]:65535"));
    BOOST_CHECK(TestParse("[::]:8333", "[::]:8333"));
    BOOST_CHECK(TestParse("[127.0.0.1]", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse(":::", "[::]:0"));

    // verify that an internal address fails to resolve
    BOOST_CHECK(TestParse("[fd6b:88c0:8724:1:2:3:4:5]", "[::]:0"));
    // and that a one-off resolves correctly
    BOOST_CHECK(TestParse("[fd6c:88c0:8724:1:2:3:4:5]", "[fd6c:88c0:8724:1:2:3:4:5]:65535"));
}

BOOST_AUTO_TEST_CASE(onioncat_test)
{
    // values from https://web.archive.org/web/20121122003543/http://www.cypherpunk.at/onioncat/wiki/OnionCat
    CNetAddr addr1(ResolveIP("5wyqrzbvrdsumnok.onion"));
    CNetAddr addr2(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca"));
    BOOST_CHECK(addr1 == addr2);
    BOOST_CHECK(addr1.IsTor());
    BOOST_CHECK(addr1.ToStringIP() == "5wyqrzbvrdsumnok.onion");
    BOOST_CHECK(addr1.IsRoutable());
}

BOOST_AUTO_TEST_CASE(embedded_test)
{
    CNetAddr addr1(ResolveIP("1.2.3.4"));
    CNetAddr addr2(ResolveIP("::FFFF:0102:0304"));
    BOOST_CHECK(addr2.IsIPv4());
    BOOST_CHECK_EQUAL(addr1.ToString(), addr2.ToString());
}

BOOST_AUTO_TEST_CASE(subnet_test)
{

    BOOST_CHECK(ResolveSubNet("1.2.3.0/24") == ResolveSubNet("1.2.3.0/255.255.255.0"));
    BOOST_CHECK(ResolveSubNet("1.2.3.0/24") != ResolveSubNet("1.2.4.0/255.255.255.0"));
    BOOST_CHECK(ResolveSubNet("1.2.3.0/24").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(!ResolveSubNet("1.2.2.0/24").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(ResolveSubNet("1.2.3.4").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(ResolveSubNet("1.2.3.4/32").Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(!ResolveSubNet("1.2.3.4").Match(ResolveIP("5.6.7.8")));
    BOOST_CHECK(!ResolveSubNet("1.2.3.4/32").Match(ResolveIP("5.6.7.8")));
    BOOST_CHECK(ResolveSubNet("::ffff:127.0.0.1").Match(ResolveIP("127.0.0.1")));
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:8").Match(ResolveIP("1:2:3:4:5:6:7:8")));
    BOOST_CHECK(!ResolveSubNet("1:2:3:4:5:6:7:8").Match(ResolveIP("1:2:3:4:5:6:7:9")));
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:0/112").Match(ResolveIP("1:2:3:4:5:6:7:1234")));
    BOOST_CHECK(ResolveSubNet("192.168.0.1/24").Match(ResolveIP("192.168.0.2")));
    BOOST_CHECK(ResolveSubNet("192.168.0.20/29").Match(ResolveIP("192.168.0.18")));
    BOOST_CHECK(ResolveSubNet("1.2.2.1/24").Match(ResolveIP("1.2.2.4")));
    BOOST_CHECK(ResolveSubNet("1.2.2.110/31").Match(ResolveIP("1.2.2.111")));
    BOOST_CHECK(ResolveSubNet("1.2.2.20/26").Match(ResolveIP("1.2.2.63")));
    // All-Matching IPv6 Matches arbitrary IPv6
    BOOST_CHECK(ResolveSubNet("::/0").Match(ResolveIP("1:2:3:4:5:6:7:1234")));
    // Addresses from one network (IPv4) don't belong to subnets of another network (IPv6)
    BOOST_CHECK(!ResolveSubNet("::/0").Match(ResolveIP("1.2.3.4")));
    // All-Matching IPv4 does not Match IPv6
    BOOST_CHECK(!ResolveSubNet("0.0.0.0/0").Match(ResolveIP("1:2:3:4:5:6:7:1234")));
    // Invalid subnets Match nothing (not even invalid addresses)
    BOOST_CHECK(!CSubNet().Match(ResolveIP("1.2.3.4")));
    BOOST_CHECK(!ResolveSubNet("").Match(ResolveIP("4.5.6.7")));
    BOOST_CHECK(!ResolveSubNet("bloop").Match(ResolveIP("0.0.0.0")));
    BOOST_CHECK(!ResolveSubNet("bloop").Match(ResolveIP("hab")));
    // Check valid/invalid
    BOOST_CHECK(ResolveSubNet("1.2.3.0/0").IsValid());
    BOOST_CHECK(!ResolveSubNet("1.2.3.0/-1").IsValid());
    BOOST_CHECK(ResolveSubNet("1.2.3.0/32").IsValid());
    BOOST_CHECK(!ResolveSubNet("1.2.3.0/33").IsValid());
    BOOST_CHECK(!ResolveSubNet("1.2.3.0/300").IsValid());
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:8/0").IsValid());
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:8/33").IsValid());
    BOOST_CHECK(!ResolveSubNet("1:2:3:4:5:6:7:8/-1").IsValid());
    BOOST_CHECK(ResolveSubNet("1:2:3:4:5:6:7:8/128").IsValid());
    BOOST_CHECK(!ResolveSubNet("1:2:3:4:5:6:7:8/129").IsValid());
    BOOST_CHECK(!ResolveSubNet("fuzzy").IsValid());

    //CNetAddr constructor test
    BOOST_CHECK(CSubNet(ResolveIP("127.0.0.1")).IsValid());
    BOOST_CHECK(CSubNet(ResolveIP("127.0.0.1")).Match(ResolveIP("127.0.0.1")));
    BOOST_CHECK(!CSubNet(ResolveIP("127.0.0.1")).Match(ResolveIP("127.0.0.2")));
    BOOST_CHECK(CSubNet(ResolveIP("127.0.0.1")).ToString() == "127.0.0.1/32");

    CSubNet subnet = CSubNet(ResolveIP("1.2.3.4"), 32);
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.4/32");
    subnet = CSubNet(ResolveIP("1.2.3.4"), 8);
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/8");
    subnet = CSubNet(ResolveIP("1.2.3.4"), 0);
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/0");

    subnet = CSubNet(ResolveIP("1.2.3.4"), ResolveIP("255.255.255.255"));
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.4/32");
    subnet = CSubNet(ResolveIP("1.2.3.4"), ResolveIP("255.0.0.0"));
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/8");
    subnet = CSubNet(ResolveIP("1.2.3.4"), ResolveIP("0.0.0.0"));
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/0");

    BOOST_CHECK(CSubNet(ResolveIP("1:2:3:4:5:6:7:8")).IsValid());
    BOOST_CHECK(CSubNet(ResolveIP("1:2:3:4:5:6:7:8")).Match(ResolveIP("1:2:3:4:5:6:7:8")));
    BOOST_CHECK(!CSubNet(ResolveIP("1:2:3:4:5:6:7:8")).Match(ResolveIP("1:2:3:4:5:6:7:9")));
    BOOST_CHECK(CSubNet(ResolveIP("1:2:3:4:5:6:7:8")).ToString() == "1:2:3:4:5:6:7:8/128");
    // IPv4 address with IPv6 netmask or the other way around.
    BOOST_CHECK(!CSubNet(ResolveIP("1.1.1.1"), ResolveIP("ffff::")).IsValid());
    BOOST_CHECK(!CSubNet(ResolveIP("::1"), ResolveIP("255.0.0.0")).IsValid());
    // Can't subnet TOR (or any other non-IPv4 and non-IPv6 network).
    BOOST_CHECK(!CSubNet(ResolveIP("5wyqrzbvrdsumnok.onion"), ResolveIP("255.0.0.0")).IsValid());

    subnet = ResolveSubNet("1.2.3.4/255.255.255.255");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.4/32");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.254");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.4/31");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.252");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.4/30");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.248");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/29");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.240");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/28");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.224");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/27");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.192");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/26");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.128");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/25");
    subnet = ResolveSubNet("1.2.3.4/255.255.255.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.3.0/24");
    subnet = ResolveSubNet("1.2.3.4/255.255.254.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.2.0/23");
    subnet = ResolveSubNet("1.2.3.4/255.255.252.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/22");
    subnet = ResolveSubNet("1.2.3.4/255.255.248.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/21");
    subnet = ResolveSubNet("1.2.3.4/255.255.240.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/20");
    subnet = ResolveSubNet("1.2.3.4/255.255.224.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/19");
    subnet = ResolveSubNet("1.2.3.4/255.255.192.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/18");
    subnet = ResolveSubNet("1.2.3.4/255.255.128.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/17");
    subnet = ResolveSubNet("1.2.3.4/255.255.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/16");
    subnet = ResolveSubNet("1.2.3.4/255.254.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.2.0.0/15");
    subnet = ResolveSubNet("1.2.3.4/255.252.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/14");
    subnet = ResolveSubNet("1.2.3.4/255.248.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/13");
    subnet = ResolveSubNet("1.2.3.4/255.240.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/12");
    subnet = ResolveSubNet("1.2.3.4/255.224.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/11");
    subnet = ResolveSubNet("1.2.3.4/255.192.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/10");
    subnet = ResolveSubNet("1.2.3.4/255.128.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/9");
    subnet = ResolveSubNet("1.2.3.4/255.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1.0.0.0/8");
    subnet = ResolveSubNet("1.2.3.4/254.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/7");
    subnet = ResolveSubNet("1.2.3.4/252.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/6");
    subnet = ResolveSubNet("1.2.3.4/248.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/5");
    subnet = ResolveSubNet("1.2.3.4/240.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/4");
    subnet = ResolveSubNet("1.2.3.4/224.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/3");
    subnet = ResolveSubNet("1.2.3.4/192.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/2");
    subnet = ResolveSubNet("1.2.3.4/128.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/1");
    subnet = ResolveSubNet("1.2.3.4/0.0.0.0");
    BOOST_CHECK_EQUAL(subnet.ToString(), "0.0.0.0/0");

    subnet = ResolveSubNet("1:2:3:4:5:6:7:8/ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1:2:3:4:5:6:7:8/128");
    subnet = ResolveSubNet("1:2:3:4:5:6:7:8/ffff:0000:0000:0000:0000:0000:0000:0000");
    BOOST_CHECK_EQUAL(subnet.ToString(), "1::/16");
    subnet = ResolveSubNet("1:2:3:4:5:6:7:8/0000:0000:0000:0000:0000:0000:0000:0000");
    BOOST_CHECK_EQUAL(subnet.ToString(), "::/0");
    subnet = ResolveSubNet("1.2.3.4/255.255.232.0");
    BOOST_CHECK(!subnet.IsValid());
    BOOST_CHECK(!ResolveSubNet("1.2.3.4/255.0.255.255").IsValid());
    subnet = ResolveSubNet("1:2:3:4:5:6:7:8/ffff:ffff:ffff:fffe:ffff:ffff:ffff:ff0f");
    BOOST_CHECK(!subnet.IsValid());
}

BOOST_AUTO_TEST_CASE(netbase_getgroup)
{
    BOOST_CHECK(ResolveIP("127.0.0.1").GetGroup() == std::vector<unsigned char>({0})); // Local -> !Routable()
    BOOST_CHECK(ResolveIP("257.0.0.1").GetGroup() == std::vector<unsigned char>({0})); // !Valid -> !Routable()
    BOOST_CHECK(ResolveIP("10.0.0.1").GetGroup() == std::vector<unsigned char>({0})); // RFC1918 -> !Routable()
    BOOST_CHECK(ResolveIP("169.254.1.1").GetGroup() == std::vector<unsigned char>({0})); // RFC3927 -> !Routable()
    BOOST_CHECK(ResolveIP("1.2.3.4").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // IPv4
    BOOST_CHECK(ResolveIP("::FFFF:0:102:304").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC6145
    BOOST_CHECK(ResolveIP("64:FF9B::102:304").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC6052
    BOOST_CHECK(ResolveIP("2002:102:304:9999:9999:9999:9999:9999").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC3964
    BOOST_CHECK(ResolveIP("2001:0:9999:9999:9999:9999:FEFD:FCFB").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC4380
    BOOST_CHECK(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_TOR, 239})); // Tor
    BOOST_CHECK(ResolveIP("2001:470:abcd:9999:9999:9999:9999:9999").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV6, 32, 1, 4, 112, 175})); //he.net
    BOOST_CHECK(ResolveIP("2001:2001:9999:9999:9999:9999:9999:9999").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV6, 32, 1, 32, 1})); //IPv6

    // baz.net sha256 hash: 12929400eb4607c4ac075f087167e75286b179c693eb059a01774b864e8fe505
    std::vector<unsigned char> internal_group = {NET_INTERNAL, 0x12, 0x92, 0x94, 0x00, 0xeb, 0x46, 0x07, 0xc4, 0xac, 0x07};
    BOOST_CHECK(CreateInternal("baz.net").GetGroup() == internal_group);
}


BOOST_AUTO_TEST_CASE(subnet_cidr_uint8_bounds)
{
    uint8_t value = 42;
    BOOST_CHECK(ParseUInt8("0", &value));
    BOOST_CHECK_EQUAL(value, 0);
    BOOST_CHECK(ParseUInt8("255", &value));
    BOOST_CHECK_EQUAL(value, 255);
    for (const std::string& invalid : {"", "-1", "256", "300", "65536", "4294967295", " 1", "1 ", "1x"}) {
        value = 42;
        BOOST_CHECK(!ParseUInt8(invalid, &value));
        BOOST_CHECK_EQUAL(value, 42);
    }
    BOOST_CHECK(!ParseUInt8(std::string("1\0x", 3), &value));
    BOOST_CHECK(ParseUInt8("128", nullptr));
    for (const char* suffix : {"-1", "256", "300", "65536", "4294967295"}) {
        BOOST_CHECK(!ResolveSubNet((std::string("1.2.3.4/") + suffix).c_str()).IsValid());
        BOOST_CHECK(!ResolveSubNet((std::string("2001:470::1/") + suffix).c_str()).IsValid());
    }
}

// ---- ported from bitcoin#19845 (src/test/net_tests.cpp) ----

namespace {
class HasReason {
public:
    explicit HasReason(const std::string& reason) : m_reason(reason) {}
    template <typename E>
    bool operator() (const E& e) const {
        return std::string(e.what()).find(m_reason) != std::string::npos;
    };
private:
    const std::string m_reason;
};

void Feed(CDataStream& s, const char* hex)
{
    const std::vector<unsigned char> v = ParseHex(hex);
    s.write(MakeByteSpan(v));
}

std::string StreamHex(const CDataStream& s) { return HexStr(s.str()); }
} // namespace

BOOST_AUTO_TEST_CASE(cnetaddr_torv3_parse)
{
    CNetAddr addr;

    BOOST_REQUIRE(addr.SetSpecial("6hzph5hv6337r6p2.onion"));
    BOOST_REQUIRE(addr.IsValid());
    BOOST_REQUIRE(addr.IsTor());
    BOOST_CHECK_EQUAL(addr.ToString(), "6hzph5hv6337r6p2.onion");

    const char* torv3_addr = "pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscryd.onion";
    BOOST_REQUIRE(addr.SetSpecial(torv3_addr));
    BOOST_REQUIRE(addr.IsValid());
    BOOST_REQUIRE(addr.IsTor());
    BOOST_CHECK_EQUAL(addr.ToString(), torv3_addr);

    CNetAddr copy;
    copy.SetIP(addr);
    BOOST_CHECK(copy == addr);
    BOOST_CHECK(addr.GetGroup() == std::vector<unsigned char>({NET_TOR, 0x7F}));
    CService service(copy, 32749);
    BOOST_CHECK_EQUAL(service.ToString(), std::string(torv3_addr) + ":32749");

    CNetAddr v2;
    BOOST_REQUIRE(v2.SetSpecial("6hzph5hv6337r6p2.onion"));
    CSubNet v2_host(v2);
    BOOST_CHECK(v2_host.Match(v2));
    BOOST_CHECK(!v2_host.Match(addr));
    BOOST_CHECK(!CSubNet(addr).IsValid());

    // wrong checksum / wrong version / embedded NUL / bogus length / bad base32
    BOOST_CHECK(!addr.SetSpecial("pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscsad.onion"));
    BOOST_CHECK(!addr.SetSpecial("pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscrye.onion"));
    BOOST_CHECK(!addr.SetSpecial(std::string{
        "pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscryd\0wtf.onion", 66}));
    BOOST_CHECK(!addr.SetSpecial(std::string{"mfrggzak.onion"}));
    BOOST_CHECK(!addr.SetSpecial(std::string{"mf*g zak.onion"}));
    BOOST_CHECK(!addr.SetSpecial("totally bogus"));
}

BOOST_AUTO_TEST_CASE(cnetaddr_serialize_v1)
{
    CNetAddr addr;
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);

    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "00000000000000000000000000000000");
    s.clear();

    BOOST_REQUIRE(LookupHost("1.2.3.4", addr, false));
    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "00000000000000000000ffff01020304");
    s.clear();

    BOOST_REQUIRE(LookupHost("1a1b:2a2b:3a3b:4a4b:5a5b:6a6b:7a7b:8a8b", addr, false));
    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "1a1b2a2b3a3b4a4b5a5b6a6b7a7b8a8b");
    s.clear();

    BOOST_REQUIRE(addr.SetSpecial("6hzph5hv6337r6p2.onion"));
    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "fd87d87eeb43f1f2f3f4f5f6f7f8f9fa");
    s.clear();

    // TORv3 cannot be expressed in V1: all zeros (gets dropped as !IsValid()).
    BOOST_REQUIRE(addr.SetSpecial("pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscryd.onion"));
    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "00000000000000000000000000000000");
    s.clear();

    addr.SetInternal("a");
    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "fd6b88c08724ca978112ca1bbdcafac2");
    s.clear();
}

BOOST_AUTO_TEST_CASE(cnetaddr_serialize_v2)
{
    CNetAddr addr;
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);

    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "021000000000000000000000000000000000");
    s.clear();

    BOOST_REQUIRE(LookupHost("1.2.3.4", addr, false));
    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "010401020304");
    s.clear();

    BOOST_REQUIRE(LookupHost("1a1b:2a2b:3a3b:4a4b:5a5b:6a6b:7a7b:8a8b", addr, false));
    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "02101a1b2a2b3a3b4a4b5a5b6a6b7a7b8a8b");
    s.clear();

    BOOST_REQUIRE(addr.SetSpecial("6hzph5hv6337r6p2.onion"));
    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "030af1f2f3f4f5f6f7f8f9fa");
    s.clear();

    BOOST_REQUIRE(addr.SetSpecial("kpgvmscirrdqpekbqjsvw5teanhatztpp2gl6eee4zkowvwfxwenqaid.onion"));
    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "042053cd5648488c4707914182655b7664034e09e66f7e8cbf1084e654eb56c5bd88");
    s.clear();

    BOOST_REQUIRE(addr.SetInternal("a"));
    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s), "0210fd6b88c08724ca978112ca1bbdcafac2");
    s.clear();
}

BOOST_AUTO_TEST_CASE(cnetaddr_unserialize_v2)
{
    CNetAddr addr;
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);

    // Valid IPv4.
    Feed(s, "01" "04" "01020304");
    s >> addr;
    BOOST_CHECK(addr.IsValid());
    BOOST_CHECK(addr.IsIPv4());
    BOOST_CHECK_EQUAL(addr.ToString(), "1.2.3.4");
    BOOST_REQUIRE(s.empty());

    // Invalid IPv4, valid length but address itself is shorter.
    Feed(s, "01" "04" "0102");
    BOOST_CHECK_EXCEPTION(s >> addr, std::ios_base::failure, HasReason("end of data"));
    BOOST_REQUIRE(!s.empty());
    s.clear();

    // Invalid IPv4, with bogus length.
    Feed(s, "01" "05" "01020304");
    BOOST_CHECK_EXCEPTION(s >> addr, std::ios_base::failure,
                          HasReason("BIP155 IPv4 address with length 5 (should be 4)"));
    BOOST_REQUIRE(!s.empty());
    s.clear();

    // Invalid IPv4, with extreme length.
    Feed(s, "01" "fd0102" "01020304");
    BOOST_CHECK_EXCEPTION(s >> addr, std::ios_base::failure,
                          HasReason("Address too long: 513 > 512"));
    BOOST_REQUIRE(!s.empty());
    s.clear();

    // Valid IPv6.
    Feed(s, "02" "10" "0102030405060708090a0b0c0d0e0f10");
    s >> addr;
    BOOST_CHECK(addr.IsValid());
    BOOST_CHECK(addr.IsIPv6());
    BOOST_CHECK_EQUAL(addr.ToString(), "102:304:506:708:90a:b0c:d0e:f10");
    BOOST_REQUIRE(s.empty());

    // Valid IPv6, contains embedded "internal".
    Feed(s, "02" "10" "fd6b88c08724ca978112ca1bbdcafac2");
    s >> addr;
    BOOST_CHECK(addr.IsInternal());
    BOOST_CHECK_EQUAL(addr.ToString(), "zklycewkdo64v6wc.internal");
    BOOST_REQUIRE(s.empty());

    // Invalid IPv6, with bogus length.
    Feed(s, "02" "04" "00");
    BOOST_CHECK_EXCEPTION(s >> addr, std::ios_base::failure,
                          HasReason("BIP155 IPv6 address with length 4 (should be 16)"));
    BOOST_REQUIRE(!s.empty());
    s.clear();

    // Invalid IPv6, contains embedded IPv4.
    Feed(s, "02" "10" "00000000000000000000ffff01020304");
    s >> addr;
    BOOST_CHECK(!addr.IsValid());
    BOOST_REQUIRE(s.empty());

    // Invalid IPv6, contains embedded TORv2.
    Feed(s, "02" "10" "fd87d87eeb430102030405060708090a");
    s >> addr;
    BOOST_CHECK(!addr.IsValid());
    BOOST_REQUIRE(s.empty());

    // Valid TORv2.
    Feed(s, "03" "0a" "f1f2f3f4f5f6f7f8f9fa");
    s >> addr;
    BOOST_CHECK(addr.IsValid());
    BOOST_CHECK(addr.IsTor());
    BOOST_CHECK_EQUAL(addr.ToString(), "6hzph5hv6337r6p2.onion");
    BOOST_REQUIRE(s.empty());

    // Invalid TORv2, with bogus length.
    Feed(s, "03" "07" "00");
    BOOST_CHECK_EXCEPTION(s >> addr, std::ios_base::failure,
                          HasReason("BIP155 TORv2 address with length 7 (should be 10)"));
    BOOST_REQUIRE(!s.empty());
    s.clear();

    // Valid TORv3 (BIP155 id 4 -> NET_TOR).
    Feed(s, "04" "20" "79bcc625184b05194975c28b66b66b04" "69f7f6556fb1ac3189a79b40dda32f1f");
    s >> addr;
    BOOST_CHECK(addr.IsValid());
    BOOST_CHECK(addr.IsTor());
    BOOST_CHECK_EQUAL(addr.ToString(),
                      "pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscryd.onion");
    BOOST_REQUIRE(s.empty());

    // Invalid TORv3, with bogus length.
    Feed(s, "04" "00" "00");
    BOOST_CHECK_EXCEPTION(s >> addr, std::ios_base::failure,
                          HasReason("BIP155 TORv3 address with length 0 (should be 32)"));
    BOOST_REQUIRE(!s.empty());
    s.clear();

    // Gridcoin adaptation: I2P (5) is unsupported -> silently ignored, !IsValid().
    Feed(s, "05" "20" "a2894dabaec08c0051a481a6dac88b64" "f98232ae42d4b6fd2fa81952dfe36a87");
    s >> addr;
    BOOST_CHECK(!addr.IsValid());
    BOOST_REQUIRE(s.empty());

    // Valid CJDNS (bitcoin#19845 test vector, enabled by the bitcoin#23077 port).
    Feed(s, "06" "10" "fc000001000200030004000500060007");
    s >> addr;
    BOOST_CHECK(addr.IsValid());
    BOOST_CHECK(addr.IsCJDNS());
    BOOST_CHECK(!addr.IsAddrV1Compatible());
    BOOST_CHECK_EQUAL(addr.ToString(), "fc00:1:2:3:4:5:6:7");
    BOOST_REQUIRE(s.empty());

    // Invalid CJDNS, wrong prefix: decoding keeps its type and bytes.
    Feed(s, "06" "10" "fd000001000200030004000500060007");
    s >> addr;
    BOOST_CHECK(addr.IsCJDNS());
    BOOST_CHECK(!addr.HasCJDNSPrefix());
    BOOST_CHECK(!addr.IsValid());
    BOOST_REQUIRE(s.empty());

    // Invalid CJDNS, wrong length.
    Feed(s, "06" "01" "00");
    BOOST_CHECK_EXCEPTION(s >> addr, std::ios_base::failure,
                          HasReason("BIP155 CJDNS address with length 1 (should be 16)"));
    BOOST_REQUIRE(!s.empty());
    s.clear();

    // Unsupported/future IDs must consume exactly their payload, regardless
    // of a nonstandard length, and leave the following supported record readable.
    for (uint8_t id : {uint8_t{5}, uint8_t{0xAA}}) {
        for (size_t length : {size_t{0}, size_t{1}, size_t{16}, size_t{32}, size_t{512}}) {
            s << id;
            WriteCompactSize(s, length);
            const std::vector<unsigned char> payload(length, 0xFC);
            s.write(MakeByteSpan(payload));
            Feed(s, "010401020304");
            s >> addr;
            BOOST_CHECK(!addr.IsValid());
            BOOST_CHECK(!addr.IsCJDNS());
            BOOST_CHECK(!addr.IsTor());
            BOOST_CHECK_EQUAL(s.size(), 6U);
            s >> addr;
            BOOST_CHECK(addr.IsIPv4());
            BOOST_CHECK_EQUAL(addr.ToString(), "1.2.3.4");
            BOOST_REQUIRE(s.empty());
        }
        Feed(s, (HexStr(std::vector<unsigned char>{id}) + "fd0102fc").c_str());
        BOOST_CHECK_EXCEPTION(s >> addr, std::ios_base::failure,
                              HasReason("Address too long: 513 > 512"));
        s.clear();
    }
    Feed(s, "0510fc00"); // A truncated ignored I2P record must still fail.
    BOOST_CHECK_EXCEPTION(s >> addr, std::ios_base::failure, HasReason("end of data"));
    s.clear();

    // The generic decoder also works with Gridcoin readers without ignore().
    const auto bytes = ParseHex("0501fc010401020304");
    SpanReader reader(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT, bytes);
    reader >> addr;
    BOOST_CHECK(!addr.IsValid());
    reader >> addr;
    BOOST_CHECK(addr.IsIPv4());
    BOOST_CHECK(reader.empty());

    CDataStream underlying(SER_NETWORK, PROTOCOL_VERSION);
    Feed(underlying, "0501fc010401020304"); // I2P remains unsupported.
    OverrideStream<CDataStream> overridden(&underlying, SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
    overridden >> addr;
    BOOST_CHECK(!addr.IsValid());
    overridden >> addr;
    BOOST_CHECK(addr.IsIPv4());
    BOOST_CHECK(underlying.empty());

    // Unknown, with extreme length.
    Feed(s, "aa" "fe00000002" "01020304050607");
    BOOST_CHECK_EXCEPTION(s >> addr, std::ios_base::failure,
                          HasReason("Address too long: 33554432 > 512"));
    BOOST_REQUIRE(!s.empty());
    s.clear();

    // Unknown, with reasonable length.
    Feed(s, "aa" "04" "01020304");
    s >> addr;
    BOOST_CHECK(!addr.IsValid());
    BOOST_REQUIRE(s.empty());

    // Unknown, with zero length.
    Feed(s, "aa" "00");
    s >> addr;
    BOOST_CHECK(!addr.IsValid());
    BOOST_REQUIRE(s.empty());
}

BOOST_AUTO_TEST_CASE(cservice_v1_bytes_unchanged)
{
    BOOST_CHECK_EQUAL(PROTOCOL_VERSION & ADDRV2_FORMAT, 0);
    BOOST_CHECK_EQUAL(INIT_PROTO_VERSION & ADDRV2_FORMAT, 0);
    BOOST_CHECK_EQUAL(DATABASE_VERSION & ADDRV2_FORMAT, 0);
    // CService/CSubNet without ADDRV2_FORMAT must stay byte-identical (peers.dat, banlist.dat, addr).
    CService svc(ResolveIP("1.2.3.4"), 32749);
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
    s << svc;
    BOOST_CHECK_EQUAL(StreamHex(s), "00000000000000000000ffff010203047fed");
}


BOOST_AUTO_TEST_CASE(cnetaddr_addrv1_compatible)
{
    CNetAddr addr;

    BOOST_CHECK(ResolveIP("12.34.56.78").IsAddrV1Compatible());
    BOOST_CHECK(ResolveIP("1122:3344:5566:7788:9900:aabb:ccdd:eeff").IsAddrV1Compatible());

    BOOST_REQUIRE(addr.SetSpecial("6hzph5hv6337r6p2.onion"));
    BOOST_CHECK(addr.IsAddrV1Compatible());

    BOOST_REQUIRE(addr.SetSpecial("pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscryd.onion"));
    BOOST_CHECK(!addr.IsAddrV1Compatible());

    BOOST_REQUIRE(addr.SetInternal("esffpvrt3wpeaygy"));
    BOOST_CHECK(addr.IsAddrV1Compatible());

    // Tor v3 GetKey()/GetAddrBytes() must be the 32 raw bytes, not 16 zero bytes,
    // so that distinct v3 peers don't collide in setAddrKnown/addrman.
    CNetAddr v3a, v3b;
    BOOST_REQUIRE(v3a.SetSpecial("pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscryd.onion"));
    {
        CDataStream s(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
        Feed(s, "04" "20" "1111111111111111111111111111111111111111111111111111111111111111");
        s >> v3b;
    }
    BOOST_REQUIRE(v3b.IsTor() && !v3b.IsAddrV1Compatible());
    BOOST_CHECK_EQUAL(v3a.GetAddrBytes().size(), 32U);
    BOOST_CHECK(CService(v3a, 32749).GetKey() != CService(v3b, 32749).GetKey());
}

// CNetAddr (un)ser is tested above; here only edge cases for port, service flags and time.
static const std::vector<CAddress> fixture_addresses({
    CAddress(CService(CNetAddr(in6addr_loopback), 0 /* port */),
             NODE_NONE,
             0x4966bc61U /* Fri Jan  9 02:54:25 UTC 2009 */),
    CAddress(CService(CNetAddr(in6addr_loopback), 0x00f1 /* port */),
             NODE_NETWORK,
             0x83766279U /* Tue Nov 22 11:22:33 UTC 2039 */),
    CAddress(CService(CNetAddr(in6addr_loopback), 0xf1f2 /* port */),
             static_cast<ServiceFlags>(0x448), // upstream used NODE_WITNESS|NODE_COMPACT_FILTERS|NODE_NETWORK_LIMITED
             0xffffffffU /* Sun Feb  7 06:28:15 UTC 2106 */)
});

static constexpr const char* stream_addrv1_hex =
    "03"
    "61bc6649" "0000000000000000" "00000000000000000000000000000001" "0000"
    "79627683" "0100000000000000" "00000000000000000000000000000001" "00f1"
    "ffffffff" "4804000000000000" "00000000000000000000000000000001" "f1f2";

static constexpr const char* stream_addrv2_hex =
    "03"
    "61bc6649" "00"     "02" "10" "00000000000000000000000000000001" "0000"
    "79627683" "01"     "02" "10" "00000000000000000000000000000001" "00f1"
    "ffffffff" "fd4804" "02" "10" "00000000000000000000000000000001" "f1f2";

BOOST_AUTO_TEST_CASE(caddress_serialize_v1)
{
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
    s << fixture_addresses;
    BOOST_CHECK_EQUAL(StreamHex(s), stream_addrv1_hex);
}

BOOST_AUTO_TEST_CASE(caddress_unserialize_v1)
{
    CDataStream s(ParseHex(stream_addrv1_hex), SER_NETWORK, PROTOCOL_VERSION);
    std::vector<CAddress> addresses_unserialized;
    s >> addresses_unserialized;
    BOOST_REQUIRE_EQUAL(addresses_unserialized.size(), fixture_addresses.size());
    for (size_t i = 0; i < fixture_addresses.size(); ++i) {
        BOOST_CHECK(fixture_addresses[i] == addresses_unserialized[i]);
        BOOST_CHECK_EQUAL(addresses_unserialized[i].nServices, fixture_addresses[i].nServices);
        BOOST_CHECK_EQUAL(addresses_unserialized[i].nTime, fixture_addresses[i].nTime);
    }
    BOOST_CHECK(s.empty());
}

BOOST_AUTO_TEST_CASE(caddress_serialize_v2)
{
    CDataStream s(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
    s << fixture_addresses;
    BOOST_CHECK_EQUAL(StreamHex(s), stream_addrv2_hex);

    // Service flags use the full uint64_t domain, not the vector-size cap.
    CAddress large_services = fixture_addresses[0];
    large_services.nServices = static_cast<ServiceFlags>(UINT64_MAX);
    CDataStream flags(SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
    flags << large_services;
    CAddress decoded;
    flags >> decoded;
    BOOST_CHECK_EQUAL(static_cast<uint64_t>(decoded.nServices), UINT64_MAX);
    BOOST_CHECK_EQUAL(decoded.nTime, large_services.nTime);
    BOOST_CHECK(decoded == large_services);
    BOOST_CHECK(flags.empty());
}

BOOST_AUTO_TEST_CASE(caddress_unserialize_v2)
{
    CDataStream s(ParseHex(stream_addrv2_hex), SER_NETWORK, PROTOCOL_VERSION | ADDRV2_FORMAT);
    std::vector<CAddress> addresses_unserialized;
    s >> addresses_unserialized;
    BOOST_REQUIRE_EQUAL(addresses_unserialized.size(), fixture_addresses.size());
    for (size_t i = 0; i < fixture_addresses.size(); ++i) {
        BOOST_CHECK(fixture_addresses[i] == addresses_unserialized[i]);
        BOOST_CHECK_EQUAL(addresses_unserialized[i].nServices, fixture_addresses[i].nServices);
        BOOST_CHECK_EQUAL(addresses_unserialized[i].nTime, fixture_addresses[i].nTime);
    }
    BOOST_CHECK(s.empty());
}

// bitcoin#20516: well-defined CAddress disk serialization.
BOOST_AUTO_TEST_CASE(caddress_disk_v1_format)
{
    const CAddress addr(CService(CNetAddr(in6addr_loopback), 0x00f1), NODE_NETWORK, 0x83766279U);

    // Written without ADDRV2_FORMAT: fixed DISK_VERSION_INIT (5000000 = 0x004c4b40), then V1 body.
    CDataStream s(SER_DISK, CLIENT_VERSION);
    s << addr;
    BOOST_CHECK_EQUAL(StreamHex(s),
        "404b4c00" "79627683" "0100000000000000" "00000000000000000000000000000001" "00f1");
    CAddress back;
    s >> back;
    BOOST_CHECK(back == addr);

    // Old Gridcoin files stored the real CLIENT_VERSION (> 2^19, e.g. 5.4.2.0 = 5040200 =
    // 0x004ce848) in this field. It must still read as V1, with or without ADDRV2_FORMAT.
    for (int ver : {CLIENT_VERSION, CLIENT_VERSION | ADDRV2_FORMAT}) {
        CDataStream old(SER_DISK, ver);
        Feed(old, "48e84c00" "79627683" "0100000000000000" "00000000000000000000000000000001" "00f1");
        CAddress a;
        old >> a;
        BOOST_CHECK(a == addr);
    }
}

BOOST_AUTO_TEST_CASE(caddress_disk_v2_format)
{
    CNetAddr v3;
    BOOST_REQUIRE(v3.SetSpecial("pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscryd.onion"));
    const CAddress addr(CService(v3, 32749), NODE_NETWORK, 0x61bc6649U);

    CDataStream s(SER_DISK, CLIENT_VERSION | ADDRV2_FORMAT);
    s << addr;
    // DISK_VERSION_INIT | DISK_VERSION_ADDRV2 = 0x204c4b40, CompactSize services, BIP155 Tor v3.
    BOOST_CHECK_EQUAL(StreamHex(s).substr(0, 8 + 8 + 2 + 4), "404b4c20" "4966bc61" "01" "0420");
    const std::string bytes = s.str();

    CAddress back;
    s >> back;
    BOOST_CHECK(back == addr);
    BOOST_CHECK(back.IsTor() && !back.IsAddrV1Compatible());

    // A V2-flagged record must not be read by a stream that doesn't permit ADDRV2.
    CDataStream no_v2(SER_DISK, CLIENT_VERSION);
    no_v2.write(MakeByteSpan(bytes));
    CAddress tmp;
    BOOST_CHECK_THROW(no_v2 >> tmp, std::ios_base::failure);
}

BOOST_AUTO_TEST_CASE(caddress_disk_unknown_format)
{
    // Bits above ADDRV2 (here 1<<30) are reserved; reject instead of guessing.
    CDataStream s(SER_DISK, CLIENT_VERSION | ADDRV2_FORMAT);
    Feed(s, "404b4c40" "79627683" "0100000000000000" "00000000000000000000000000000001" "00f1");
    CAddress a;
    BOOST_CHECK_THROW(s >> a, std::ios_base::failure);
}

BOOST_AUTO_TEST_SUITE_END()
