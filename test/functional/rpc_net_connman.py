#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Functional coverage for CConnman node-access RPCs (#2558) not exercised
elsewhere in the suite:

  * addnode add/remove list management + the ALREADY_ADDED / NOT_ADDED error
    codes (PR 9b2: AddNode / RemoveAddedNode)
  * getnodeaddresses                  (PR 9d4: addrman owned by CConnman)
  * setban / listbanned / clearbanned (PR 9b: setban -> CConnman::DisconnectNode)

getconnectioncount / getpeerinfo / getnettotals / block relay are already covered
by rpc_net.py and rpc_netinfo.py, and getaddednodeinfo's output shape by
rpc_getaddednodeinfo.py, so they are not repeated here. A single investor-mode
regtest node suffices: none of these calls needs a live peer.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

# RPC error codes (src/rpc/protocol.h)
RPC_CLIENT_NODE_ALREADY_ADDED = -23
RPC_CLIENT_NODE_NOT_ADDED = -24


class RpcNetConnmanTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]
        self.test_addnode_list(node)
        self.test_getnodeaddresses(node)
        self.test_setban(node)

    def test_addnode_list(self, node):
        """PR 9b2: addnode add/remove list management + distinct error codes."""
        self.log.info("addnode add/remove + ALREADY_ADDED / NOT_ADDED")
        dummy = "192.0.2.10:32749"  # TEST-NET-1, never reachable

        # add -> adding again is rejected with the distinct ALREADY_ADDED code
        node.addnode(dummy, "add")
        assert_raises_rpc_error(RPC_CLIENT_NODE_ALREADY_ADDED, "already added",
                                node.addnode, dummy, "add")

        # remove -> removing again is rejected with NOT_ADDED
        node.addnode(dummy, "remove")
        assert_raises_rpc_error(RPC_CLIENT_NODE_NOT_ADDED, "has not been added",
                                node.addnode, dummy, "remove")
        self.log.info("addnode list management OK")

    def test_getnodeaddresses(self, node):
        """PR 9d4: getnodeaddresses reads the CConnman-owned addrman."""
        self.log.info("getnodeaddresses")
        # On a fresh regtest node addrman is empty; the call must still succeed
        # and return a list (not error now that addrman lives on CConnman).
        addrs = node.getnodeaddresses()
        assert_equal(isinstance(addrs, list), True)
        self.log.info("getnodeaddresses returned %d address(es)", len(addrs))

    def test_setban(self, node):
        """PR 9b: setban / listbanned / clearbanned."""
        self.log.info("setban / listbanned / clearbanned")
        subnet = "192.0.2.0/24"

        assert_equal(node.listbanned(), [])
        node.setban(subnet, "add")
        banned = node.listbanned()
        assert_equal(len(banned), 1)
        assert_equal(banned[0]["address"], subnet)

        # banning the same subnet again is rejected
        assert_raises_rpc_error(RPC_CLIENT_NODE_ALREADY_ADDED, "already banned",
                                node.setban, subnet, "add")

        node.clearbanned()
        assert_equal(node.listbanned(), [])
        self.log.info("ban list management OK")


if __name__ == "__main__":
    RpcNetConnmanTest().main()
