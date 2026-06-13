#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""RPC network-information queries on regtest.

Investor-mode only, read-only. Two nodes start unconnected; asserts the
network-introspection surface before and after they are linked:

  - getnetworkinfo exposes version/protocolversion/connections/errors and the
    connection count tracks the link;
  - getnettotals reports byte counters + timemillis;
  - getconnectioncount goes 0 -> >=1 on connect;
  - getpeerinfo lists the peer with Gridcoin's field set (incl. nTrust).
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than_or_equal


class RpcNetInfoTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"], ["-staking=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node0, node1 = self.nodes

        # --- getnetworkinfo / getnettotals while unconnected ---
        netinfo = node0.getnetworkinfo()
        for field in ("version", "protocolversion", "timeoffset", "connections", "errors"):
            assert field in netinfo, (field, netinfo)
        assert_equal(netinfo["connections"], 0)

        totals = node0.getnettotals()
        for field in ("totalbytesrecv", "totalbytessent", "timemillis"):
            assert field in totals, (field, totals)

        assert_equal(node0.getconnectioncount(), 0)
        self.log.info("unconnected node: getnetworkinfo/getnettotals/getconnectioncount OK")

        # --- link the nodes, then inspect peer state ---
        self.connect_nodes(0, 1)
        assert_greater_than_or_equal(node0.getconnectioncount(), 1)
        assert_greater_than_or_equal(node0.getnetworkinfo()["connections"], 1)

        peers = node0.getpeerinfo()
        assert len(peers) >= 1, peers
        peer = peers[0]
        for field in ("id", "addr", "version", "subver", "inbound", "conntime", "nTrust"):
            assert field in peer, (field, peer)
        self.log.info("getpeerinfo reports the linked peer with the expected field set")


if __name__ == "__main__":
    RpcNetInfoTest().main()
