#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A repeated stop of a node that exited non-zero reports that exit.

TestNode.stop_node() closes the node's stdout/stderr handles in its finally.
The node stays marked running until is_node_stopped() sees it exit with code 0.
So a node that exited non-zero, or whose first stop_node() raised before
wait_until_stopped() ran, is stopped a second time by the framework's next
stop_nodes(), for example the one in shutdown(). That second stop_node() must
not seek the closed stderr ("ValueError: seek of closed file"), which
stop_nodes() would report in place of the node's non-zero exit.

Each case kills its node (a non-zero exit), stops it once, and asserts that the
next stop_nodes() raises is_node_stopped()'s non-zero-exit assertion:

- a connected node whose stop RPC fails because the process is gone. The
  framework's stop_node() then raises before it calls wait_until_stopped(),
  the restart_node() path;
- a node the framework never connected to, stopped by stop_nodes() twice, the
  start_nodes() failure path followed by shutdown().

Each case then marks its node stopped, as assert_start_raises_init_error()
does, so the framework's own shutdown does not report the exit this test caused
on purpose. The stop_node() and wait_until_stopped() errors logged along the
way are expected.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_raises, assert_raises_message


class FeatureStopNodeTwiceTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        # Under --usecli the stop goes through gridcoin-cli and raises
        # CalledProcessError, which stop_node() does not catch.
        self.supports_cli = False

    def setup_network(self):
        # Add both nodes but start neither: each case starts its own node.
        self.add_nodes(self.num_nodes)

    def kill_node(self, i):
        node = self.nodes[i]
        node.process.kill()
        self.wait_for_node_exit(i, timeout=60)
        assert node.process.returncode != 0

    def mark_node_stopped(self, node):
        # Clear what is_node_stopped() clears on a clean exit.
        assert node.process.poll() is not None
        node.running = False
        node.process = None
        node.rpc_connected = False
        node.rpc = None

    def run_test(self):
        self.stop_after_failed_stop_rpc()
        self.stop_nodes_twice_never_connected()

    def stop_after_failed_stop_rpc(self):
        node = self.nodes[0]
        self.start_node(0)
        assert node.rpc_connected
        self.kill_node(0)

        self.log.info("Connected node: the stop RPC fails, so stop_node() raises before wait_until_stopped()")
        assert_raises(OSError, self.stop_node, 0)
        assert node.running and node.stderr.closed

        self.log.info("The next stop_nodes() reports the non-zero exit")
        assert_raises_message(AssertionError, "non-zero exit code", self.stop_nodes)
        self.mark_node_stopped(node)

    def stop_nodes_twice_never_connected(self):
        node = self.nodes[1]
        node.start()
        self.kill_node(1)
        assert not node.rpc_connected

        self.log.info("Never-connected node: the first stop_nodes() raises ConnectionError")
        assert_raises_message(ConnectionError, "no RPC connection", self.stop_nodes)
        assert node.running and node.stderr.closed

        self.log.info("The second stop_nodes() reports the non-zero exit")
        assert_raises_message(AssertionError, "non-zero exit code", self.stop_nodes)
        self.mark_node_stopped(node)


if __name__ == "__main__":
    FeatureStopNodeTwiceTest().main()
