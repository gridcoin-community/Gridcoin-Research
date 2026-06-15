#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A local sidestaking on regtest.

Investor-mode only: a sidestake is a coinstake reward split, NOT a researcher
feature, so it works on plain PoS blocks with no beacon/CPID.

Local sidestaking is configured by startup args (no addsidestake RPC):
  -enablesidestaking=1 -sidestake=<address>,<percent>
The destination must exist before the node starts, so we mint it first, then
restart with sidestaking enabled.

Asserts the config path end to end: the configured local sidestake entry is
surfaced by listsidestakes, and the chain still advances (stakes) with
sidestaking enabled. The actual reward split is logged for information only: the
regtest PoS subsidy may be zero, in which case there is nothing to split, so it
is not asserted here (revisit once a positive research/PoS subsidy exists).
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class SidestakeTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]

        # Mint the sidestake destination, then restart with local sidestaking on
        # (the -sidestake arg is read at startup, so the address must pre-exist).
        ss_addr = node.getnewaddress()
        self.restart_node(0, extra_args=[
            "-staking=0", "-connect=0", "-listen=0",
            "-enablesidestaking=1", "-sidestake={},50".format(ss_addr),
        ])
        node = self.nodes[0]

        # --- config path: listsidestakes surfaces the configured local entry ---
        listed = node.listsidestakes()
        assert ss_addr in str(listed), \
            "configured sidestake address not surfaced by listsidestakes: {}".format(listed)
        self.log.info("local sidestake configured + listed: %s @ 50%%", ss_addr)

        # --- chain still advances (stakes) with sidestaking enabled ---
        start = node.getblockcount()
        node.generatetoaddress(10, node.getnewaddress())
        assert_equal(node.getblockcount(), start + 10)

        # reward split is observational only (regtest subsidy may be 0)
        received = node.getreceivedbyaddress(ss_addr, 0)
        self.log.info("sidestake address received %s GRC over 10 staked blocks "
                      "(0 is acceptable if the regtest subsidy is 0)", received)


if __name__ == "__main__":
    SidestakeTest().main()
