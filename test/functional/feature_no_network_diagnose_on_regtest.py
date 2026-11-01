#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""walletdiagnose on regtest must not reach outside hosts (issue #3359).

Two of its checks talk to the internet without a chain gate: VerifyClock sends
a UDP query to the public NTP pool, and VerifyTCPPort resolves and connects to
a port test site, so on regtest they went out like a mainnet node's. (The
third, the client version check, is already mainnet-only.)
The Diagnostics dialog in the GUI runs the same two classes.

Both now report "NA" on regtest, with a description saying why. The node here
has no peers, so the connection-count check fails -- which is exactly the state
in which both checks go to the network rather than answering from peer data.
That precondition is asserted first: without it the "NA" results could come
from a shortcut rather than from the gate.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class NoNetworkDiagnoseOnRegtestTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]
        assert_equal(node.getconnectioncount(), 0)

        report = node.walletdiagnose()

        self.log.info("with no peers, both checks would take their network path")
        assert_equal(report["check_connection_count"]["result"], "FAIL")

        self.log.info("the NTP clock check is not run on regtest")
        assert_equal(report["verify_clock"]["result"], "NA")
        assert "regtest" in report["verify_clock"]["desc"], report["verify_clock"]

        self.log.info("the TCP port check is not run on regtest")
        assert_equal(report["verify_tcp_port"]["result"], "NA")
        assert "regtest" in report["verify_tcp_port"]["desc"], report["verify_tcp_port"]


if __name__ == '__main__':
    NoNetworkDiagnoseOnRegtestTest().main()
