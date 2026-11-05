#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Functional coverage for the dumpcontracts height range check.

dumpcontracts walks back from the chain tip to the requested heights. A high_height above
the tip, or a low_height that is not on the chain, left the block index pointer null and
crashed the node. Both are now refused with RPC_INVALID_PARAMETER (-8) before the output
file is created. A valid range must still produce its file.
"""

import os

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


class RpcDumpContractsTest(GridcoinTestFramework):
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
        node.generatetoaddress(3, node.getnewaddress())
        assert_equal(node.getblockcount(), 3)

        out = os.path.join(self.options.tmpdir, "contracts.txt")

        self.log.info("high_height above the tip is refused")
        assert_raises_rpc_error(-8, "high_height 103 is not on the chain: heights run from 0 to 3",
                                node.dumpcontracts, "*", out, True, 0, 103)
        assert_equal(os.path.exists(out), False)

        self.log.info("low_height above the tip is refused")
        assert_raises_rpc_error(-8, "low_height 50 is not on the chain: heights run from 0 to 3",
                                node.dumpcontracts, "*", out, True, 50)
        assert_equal(os.path.exists(out), False)

        self.log.info("negative low_height is refused")
        assert_raises_rpc_error(-8, "low_height -1 is not on the chain: heights run from 0 to 3",
                                node.dumpcontracts, "*", out, True, -1)
        assert_equal(os.path.exists(out), False)

        # The node is still running and answering.
        assert_equal(node.getblockcount(), 3)

        self.log.info("valid ranges still work")
        result = node.dumpcontracts("*", out, True, 1, 3)
        assert_equal(result["low_height"], 1)
        assert_equal(result["high_height"], 3)
        assert_equal(os.path.exists(out), True)
        os.remove(out)

        result = node.dumpcontracts("*", out, True)
        assert_equal(result["high_height"], 3)
        assert_equal(os.path.exists(out), True)


if __name__ == "__main__":
    RpcDumpContractsTest().main()
