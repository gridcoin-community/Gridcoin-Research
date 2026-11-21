#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A test node reads a BOINC data directory of its own, not the host's.

A node that is given no -boincdatadir looks for BOINC in the host's default
locations when it starts. The result of a test then depends on the machine it
runs on. Where BOINC is installed and the user running the tests may not read
its directory, the node reports "Could not access BOINC data directory" on
stderr, and the framework fails a node that wrote to stderr.

The framework therefore gives every node a BOINC data directory inside the
node's own data directory, holding a client_state.xml that names no project,
and passes it to the node as -boincdatadir. A test that passes -boincdatadir
itself still decides the directory for its node.
"""

import os

from test_framework.test_framework import GridcoinTestFramework


class OwnBoincDirTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [[
            "-staking=0",
            "-connect=0",
            "-listen=0",
        ]]

    def setup_network(self):
        # The node is started as the framework prepared it, with no wallet setup.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]
        own_state = os.path.join(node.datadir, "boinc", "client_state.xml")

        self.log.info("The framework gave the node a client state file of its own")
        assert os.path.isfile(own_state)

        self.log.info("At start the node reads that file and finds no project in it")
        with node.assert_debug_log(
                expected_msgs=[
                    f'loading file to string "{own_state}"',
                    "BOINC is not attached to any projects. No CPIDs loaded.",
                ],
                unexpected_msgs=["Unable to obtain BOINC CPIDs"],
                timeout=10):
            self.restart_node(0)

        self.log.info("A -boincdatadir the test passes is the one the node reads")
        other_dir = os.path.join(self.options.tmpdir, "boinc_of_the_test")
        os.makedirs(other_dir)
        other_state = os.path.join(other_dir, "client_state.xml")
        with open(other_state, "w", encoding="utf-8") as client_state:
            client_state.write("<client_state>\n</client_state>\n")

        with node.assert_debug_log(
                expected_msgs=[f'loading file to string "{other_state}"'],
                unexpected_msgs=[f'loading file to string "{own_state}"'],
                timeout=10):
            self.restart_node(0, extra_args=self.extra_args[0] + [f"-boincdatadir={other_dir}"])


if __name__ == "__main__":
    OwnBoincDirTest().main()
