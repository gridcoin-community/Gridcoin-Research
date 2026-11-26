#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Core developers
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""BIP155 relay and handshake ordering, ported from bitcoin#19954/#20564.

Gridcoin sends legacy addresses as gridaddr and uses NODE_NETWORK without
Bitcoin's NODE_WITNESS. Enable Tor reachability with -tor, and space inbound
connections using the framework's mock-time helper. Only the source and one
receiver stay connected in each case: Gridcoin's two-peer relay fanout includes
the source, unlike Bitcoin's, so a third peer would make selection random.
"""

import ipaddress

from test_framework.messages import CAddress, msg_addrv2, msg_sendaddrv2, NODE_NETWORK
from test_framework.p2p import P2PInterface, p2p_lock
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


TORV3 = "pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscryd.onion"


def address_ip(addr):
    # The legacy framework exposes IPv4 as an integer; BIP155 uses text.
    return str(ipaddress.IPv4Address(addr.ip)) if addr.net == CAddress.NET_IPV4 else addr.ip


class AddrPeer(P2PInterface):
    def peer_connect_send_version(self):
        super().peer_connect_send_version()
        # Gridcoin only stores/relays addresses from peers at >= half our height.
        self.on_connection_send_msg.nStartingHeight = 0


class AddrReceiver(AddrPeer):
    def __init__(self, *, support_addrv2):
        super().__init__(support_addrv2=support_addrv2)
        self.legacy_messages = []
        self.v2_messages = []

    def on_addr(self, message):
        self.legacy_messages.append(message)

    def on_addrv2(self, message):
        self.v2_messages.append(message)

    def has_address(self, ip, port, *, v2):
        messages = self.v2_messages if v2 else self.legacy_messages
        return any(address_ip(a) == ip and a.port == port for m in messages for a in m.addrs)


class AddrV2RelayTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-walletbackupintervalsecs=0",
                            "-discover=0", "-tor=127.0.0.1:9"]]

    def setup_network(self):
        # No wallet/coinbase import or block generation is needed.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def addresses(self, node, ipv4, port):
        addresses = []
        for net, ip in ((CAddress.NET_IPV4, ipv4), (CAddress.NET_TORV3, TORV3)):
            addr = CAddress()
            addr.net = net
            addr.ip = ip
            addr.port = port
            addr.time = node.mock_now()
            addr.nServices = NODE_NETWORK
            addresses.append(addr)
        return addresses

    def run_test(self):
        node = self.nodes[0]
        node.setmocktime(node.mock_now())
        source = self.add_p2p_connection_spaced(node, AddrPeer())
        source.wait_until(lambda: "sendaddrv2" in source.last_message)

        self.log.info("sendaddrv2 before verack: relay IPv4 and Tor v3 as addrv2")
        with node.assert_debug_log(["signaled addrv2 support (sendaddrv2)"],
                                   unexpected_msgs=["sendaddrv2 received after verack"]):
            receiver = self.add_p2p_connection_spaced(
                node, AddrReceiver(support_addrv2=True))

        addresses = self.addresses(node, "123.123.123.1", 32749)
        message = msg_addrv2()
        # Retain the oversized-address-list check from bitcoin#19954.
        message.addrs = addresses * 501
        with node.assert_debug_log(["message addrv2 size() = 1002"]):
            source.send_and_ping(message)

        message.addrs = addresses
        source.send_and_ping(message)
        receiver.wait_until(lambda: receiver.has_address(TORV3, 32749, v2=True)
                            and receiver.has_address("123.123.123.1", 32749, v2=True))
        receiver.sync_with_ping()
        with p2p_lock:
            assert_equal(receiver.legacy_messages, [])
            relayed = [a for m in receiver.v2_messages for a in m.addrs]
            assert_equal({(a.net, a.ip, a.port, a.nServices, a.time) for a in relayed},
                         {(a.net, a.ip, a.port, a.nServices, a.time) for a in addresses})
            assert all(m.command == b"addrv2" for m in receiver.v2_messages)
        receiver.peer_disconnect()
        receiver.wait_for_disconnect()
        node.p2ps.remove(receiver)
        self.wait_until(lambda: node.num_test_p2p_connections() == 1)

        self.log.info("No sendaddrv2: relay gridaddr and omit Tor v3")
        legacy = self.add_p2p_connection_spaced(
            node, AddrReceiver(support_addrv2=False))
        message.addrs = self.addresses(node, "123.123.123.2", 32750)
        source.send_and_ping(message)
        legacy.wait_until(lambda: legacy.has_address("123.123.123.2", 32750, v2=False))
        legacy.sync_with_ping()
        with p2p_lock:
            assert_equal(legacy.v2_messages, [])
            assert all(m.command == b"gridaddr" for m in legacy.legacy_messages)
            relayed = [a for m in legacy.legacy_messages for a in m.addrs]
            assert_equal({(a.net, address_ip(a), a.port) for a in relayed},
                         {(CAddress.NET_IPV4, "123.123.123.2", 32750)})
            assert all(a.net != CAddress.NET_TORV3 and not str(a.ip).endswith(".onion")
                       for a in relayed)
        node.disconnect_p2ps()

        self.log.info("sendaddrv2 after verack: disconnect (bitcoin#20564)")
        late = self.add_p2p_connection_spaced(node, AddrPeer())
        # add_p2p_connection's ping round trip proves our verack was processed.
        with node.assert_debug_log(["sendaddrv2 received after verack", "disconnecting"]):
            late.send_message(msg_sendaddrv2())
            late.wait_for_disconnect()
        self.wait_until(lambda: node.num_test_p2p_connections() == 0)
        node.disconnect_p2ps()
        self.log.info("addrv2, gridaddr filtering, and late negotiation checks passed")


if __name__ == "__main__":
    AddrV2RelayTest().main()
