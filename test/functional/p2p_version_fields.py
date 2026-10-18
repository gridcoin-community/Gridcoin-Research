#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Peer statistics stay readable while peers handshake.

getpeerinfo copies each peer's version fields on an RPC thread, while the
message-handler thread may be storing them from that peer's version message.
This test connects and disconnects a framework peer many times while several
threads call getpeerinfo without pause.

On a ThreadSanitizer build of the daemon, a store that is not synchronized
with those reads is reported and halts the node, which fails the test. On any
build it checks that the node answers every call and ends the run with no test
peer left.
"""

import threading

from test_framework.p2p import P2PInterface
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, get_rpc_proxy

HANDSHAKES = 100
POLLERS = 3


class P2PVersionFieldsTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        # No staking, so the handshakes are the node's only work.
        self.extra_args = [["-staking=0"]]

    def setup_network(self):
        # Gridcoin has one default wallet: skip the base regtest wallet setup.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]
        stop = threading.Event()
        calls = [0] * POLLERS
        errors = []

        def poll(i):
            # One connection per thread: the proxy is not shared between threads.
            rpc = get_rpc_proxy(node.url, node.index, timeout=node.rpc_timeout)
            try:
                while not stop.is_set():
                    rpc.getpeerinfo()
                    calls[i] += 1
            except Exception as e:  # reported by the main thread below
                errors.append(repr(e))

        pollers = [threading.Thread(target=poll, args=(i,), daemon=True) for i in range(POLLERS)]
        for t in pollers:
            t.start()

        # The node accepts one inbound connection per address per five
        # seconds, so the handshakes are spaced on the mock clock.
        self.log.info("%d handshakes under %d threads calling getpeerinfo", HANDSHAKES, POLLERS)
        for _ in range(HANDSHAKES):
            self.add_p2p_connection_spaced(node, P2PInterface())
            node.disconnect_p2ps()
            if errors:
                break

        stop.set()
        for t in pollers:
            t.join()

        assert_equal(errors, [])
        assert_equal(node.num_test_p2p_connections(), 0)
        self.log.info("%d getpeerinfo calls answered", sum(calls))


if __name__ == "__main__":
    P2PVersionFieldsTest().main()
