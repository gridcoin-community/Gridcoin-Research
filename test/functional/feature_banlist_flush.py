#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""The ban list is written during Shutdown().

A manual setban add or remove is written to banlist.dat at once, but a ban
that expires only marks the list for writing; the periodic dump or the ban
manager's destructor writes it. This test lets a ban expire on the mock
clock, stops the node, and checks that the ban list was written before
Shutdown() logged that the node exited, and that the node restarts with no
bans.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class FeatureBanlistFlushTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-debug=net"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    @staticmethod
    def read_log_lines(node):
        with open(node.debug_log_path, encoding="utf-8", errors="replace") as log:
            return log.read().splitlines()

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Ban an address for 60 seconds and let the ban expire on the mock clock")
        now = node.mock_now()
        node.setmocktime(now)
        node.setban("192.0.2.1", "add", 60)
        assert_equal(len(node.listbanned()), 1)
        node.setmocktime(now + 120)
        # listbanned sweeps the expired entry, which leaves the list to be written.
        assert_equal(node.listbanned(), [])

        self.log.info("Stop the node; the ban list is written during Shutdown()")
        start = len(self.read_log_lines(node))
        self.stop_node(0)

        lines = self.read_log_lines(node)[start:]
        exited = [i for i, line in enumerate(lines) if "Gridcoin exited" in line]
        flushed = [i for i, line in enumerate(lines) if "banned node ips/subnets to banlist.dat" in line]
        assert exited, "Shutdown() did not log that the node exited"
        assert any(i < exited[0] for i in flushed), \
            "the ban list was not written to banlist.dat before Shutdown() finished"

        self.log.info("Restart; the node has no bans")
        self.start_node(0)
        assert_equal(node.listbanned(), [])


if __name__ == "__main__":
    FeatureBanlistFlushTest().main()
