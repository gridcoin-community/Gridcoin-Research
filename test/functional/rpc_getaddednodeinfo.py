#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Functional coverage for the getaddednodeinfo dns=true array fix.

dns=true previously returned {} because the handler built a UniValue object and
populated it with push_back (a no-op on objects). This guards the fix: dns=true
now returns an ARRAY of per-added-node objects, while dns=false still returns the
object mapping "addednode" -> node string. Single investor-mode regtest node; no
connection is needed since the added node is never reachable.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class RpcGetAddedNodeInfoTest(GridcoinTestFramework):
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
        dummy = "192.0.2.10:32749"  # TEST-NET-1, never reachable

        # No added nodes yet: dns=false is an empty object, dns=true an empty array.
        assert_equal(node.getaddednodeinfo(False), {})
        assert_equal(node.getaddednodeinfo(True), [])

        node.addnode(dummy, "add")

        # dns=false: object mapping "addednode" -> the node string.
        info = node.getaddednodeinfo(False)
        assert_equal(info["addednode"], dummy)

        # dns=true: ARRAY of per-added-node objects (the fix; previously {}).
        detail = node.getaddednodeinfo(True)
        assert_equal(isinstance(detail, list), True)
        assert_equal(len(detail), 1)
        assert_equal(detail[0]["addednode"], dummy)
        assert_equal(detail[0]["connected"], False)
        self.log.info("getaddednodeinfo dns=true returns an array of per-node objects")

        # Removing empties both shapes again.
        node.addnode(dummy, "remove")
        assert_equal(node.getaddednodeinfo(False), {})
        assert_equal(node.getaddednodeinfo(True), [])


if __name__ == "__main__":
    RpcGetAddedNodeInfoTest().main()
