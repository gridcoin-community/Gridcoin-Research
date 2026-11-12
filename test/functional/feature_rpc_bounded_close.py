#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A client that never reads or closes cannot hold an RPC connection open.

A raw client connects and sends nothing. The server's idle deadline
(-rpcservertimeout=2) ends the connection, and the close shuts down the send
side, waits a bounded time for the client to close, then resets the
connection. The client never closes, so without the bounded close the server
side socket would sit in FIN_WAIT2 for about a minute. The test asserts the
server side socket is gone well inside that, then that stop and restart
rebinds the RPC port while the client still holds its end.

Linux only (reads /proc).
"""

import socket
import sys
import time

from test_framework.test_framework import GridcoinTestFramework, SkipTest
from test_framework.util import rpc_port


def server_side_sockets(port, client_port):
    """Entries of /proc/net/tcp{,6} with local port `port` and remote port `client_port`."""
    found = []
    for table in ("/proc/net/tcp", "/proc/net/tcp6"):
        with open(table, encoding="utf8") as f:
            next(f)
            for line in f:
                fields = line.split()
                if (int(fields[1].rsplit(":", 1)[1], 16) == port
                        and int(fields[2].rsplit(":", 1)[1], 16) == client_port):
                    found.append(fields[3])
    return found


class RpcBoundedCloseTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0", "-rpcservertimeout=2"]]

    def skip_test_if_missing_module(self):
        if not sys.platform.startswith("linux"):
            raise SkipTest("reads /proc/net/tcp, Linux only")

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]
        port = rpc_port(0)

        raw = socket.create_connection(("127.0.0.1", port), timeout=30)
        client_port = raw.getsockname()[1]
        self.wait_until(lambda: server_side_sockets(port, client_port), timeout=10)

        # Idle deadline (2 s) plus the bounded close (2 s) is about 4 s. The bound here is far above
        # that and far below the kernel's FIN_WAIT2 timeout, so no tight timing is involved.
        deadline = time.time() + 25
        while server_side_sockets(port, client_port):
            assert time.time() < deadline, "server side socket still present: %s" % server_side_sockets(port, client_port)
            time.sleep(0.25)
        self.log.info("server side socket released while the client neither read nor closed")

        # Stop and restart while a second such client holds its socket; the port must rebind.
        raw2 = socket.create_connection(("127.0.0.1", port), timeout=30)
        self.restart_node(0, self.extra_args[0])
        node.getblockcount()
        raw.close()
        raw2.close()


if __name__ == "__main__":
    RpcBoundedCloseTest().main()
