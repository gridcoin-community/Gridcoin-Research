#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""TestNode.stop_node() must SIGTERM a node whose RPC never connected.

stop_node() sends the stop RPC through self.stop(). On a node the framework
never connected to, that goes through TestNode.__getattr__, whose connection
guard raises AssertionError. stop_node() catches only CannotSendRequest,
JSONRPCException, ConnectionError and OSError, so the AssertionError escaped
before the SIGTERM fallback could run: nothing told the daemon to exit, and
every later wait_until_stopped() spent its whole timeout on it.

The node here starts fully but serves RPC on a port the framework does not
poll (-rpcport moved up by 2000), so the framework's RPC connection is never
made while the daemon itself is healthy. The test starts it without
wait_for_rpc_connection and asserts that stop_node() raises ConnectionError
(the fallback path) rather than AssertionError, and that the daemon then exits
cleanly. It asserts the exception type and the exit, not a duration.

Why the test first waits for the moved RPC port to accept a connection: the
RPC listener opens near the end of AppInit2, after its last check for a
shutdown request. Once the port accepts, a SIGTERM lets the daemon exit 0.
Before that, a SIGTERM can make AppInit2 return false and the daemon exit 1,
which wait_until_stopped() would report as a non-zero exit code.

CLI transport is not supported: under --usecli, self.stop() goes through
gridcoin-cli and fails with CalledProcessError, which the except in
stop_node() does not catch either.
"""

import socket

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_raises_message, rpc_port


class FeatureStopNodeWithoutRpcTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.supports_cli = False

    def setup_network(self):
        # Add the node but do not start it: start_nodes() would wait for an RPC
        # connection that, by design, never comes.
        self.moved_rpc_port = rpc_port(0) + 2000
        self.add_nodes(self.num_nodes, [["-rpcport=%d" % self.moved_rpc_port]])

    def run_test(self):
        node = self.nodes[0]
        node.start()

        def moved_port_accepts():
            try:
                with socket.create_connection(('127.0.0.1', self.moved_rpc_port), timeout=1):
                    return True
            except OSError:
                return False

        self.log.info("Waiting for the daemon to serve RPC on port %d (not polled by the framework)",
                      self.moved_rpc_port)
        self.wait_until(moved_port_accepts)
        assert not node.rpc_connected

        self.log.info("stop_node() on a never-connected node raises ConnectionError and sends SIGTERM")
        assert_raises_message(ConnectionError, "no RPC connection", node.stop_node)

        # wait_until_stopped() asserts a zero exit code.
        node.wait_until_stopped()
        assert not node.running


if __name__ == "__main__":
    FeatureStopNodeWithoutRpcTest().main()
