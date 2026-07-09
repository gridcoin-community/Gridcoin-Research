#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Node shutdown must not hang on an in-service RPC connection (issue #3123).

Gridcoin's RPC server uses a synchronous boost::asio model: an accepted
connection is serviced on a worker thread that blocks in a synchronous read
(ReadHTTPRequestLine -> read_some -> recv). An idle keep-alive client, or a
client that has sent a partial request, leaves its worker parked in that recv.

StopRPCThreads() closes the listening acceptors and calls io_service->stop()
followed by worker_group->join_all(). io_service->stop() cannot wake a worker
that is parked in recv (it is not in the io_service run loop), so before the
fix join_all() blocked forever whenever such a connection was live -- the exact
hang the framework works around client-side in TestNode.stop_node() by closing
its own keep-alive socket.

This test opens a raw connection that the framework does NOT close, parks a
worker in the blocking read, issues `stop`, and asserts the daemon still exits
within a bounded time. Against a pre-fix binary the daemon hangs and
wait_until_stopped() times out; with the fix StopRPCThreads() shuts the socket
down, the read fails, the worker returns and the daemon exits cleanly.
"""

import socket
import time

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import rpc_port


class FeatureShutdownTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True

    def setup_network(self):
        # Skip the default deterministic-coinbase wallet import: it calls
        # createwallet with named args, which Gridcoin's RPC parser rejects
        # ("Params must be an array"). This test needs no wallet or chain state.
        self.add_nodes(self.num_nodes)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]

        # Raw socket straight to the RPC port. No credentials are needed: the
        # server blocks in ReadHTTPRequestLine / header parsing well before it
        # checks authorization, which is exactly the state we want to reproduce.
        host = node.rpchost or '127.0.0.1'
        port = rpc_port(node.index)

        raw = socket.create_connection((host, port), timeout=30)
        try:
            # Send a complete request line but no headers or terminating blank
            # line. The worker gets past ReadHTTPRequestLine and then parks in
            # the blocking header read, waiting for bytes that never arrive.
            raw.sendall(b'GET / HTTP/1.1\r\n')

            # Give the server a moment to accept the connection and hand it to a
            # worker that then parks in recv. If we race ahead of that, the fix
            # still handles it (RegisterRPCConnection observes the stopped latch
            # and declines to park), but for a faithful reproduction against a
            # pre-fix binary we want the worker demonstrably parked first.
            time.sleep(2 * node.timeout_factor)

            self.log.info("Parked a worker on a raw in-service RPC socket; issuing stop")
            node.stop()

            # Close the framework's own keep-alive RPC connection so the raw
            # socket above is the *only* thing that can wedge shutdown -- this
            # isolates the test to StopRPCThreads()'s ability to interrupt an
            # in-service connection (issue #3123) rather than any idle client.
            node.rpc._close_conn()

            # The daemon must exit despite the parked worker. wait_until_stopped()
            # asserts a clean (exit code 0) shutdown; a pre-fix binary hangs here.
            node.wait_until_stopped(timeout=60)
            self.log.info("Daemon shut down cleanly with an in-service RPC socket held open")
        finally:
            raw.close()


if __name__ == "__main__":
    FeatureShutdownTest().main()
