#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""An IPv4 RPC listener that cannot bind is reported, not swallowed.

With no -rpcbind the daemon listens on the loopback interfaces: [::1] first,
then 127.0.0.1 on the same port. When the IPv4 bind failed after the IPv6 one
succeeded, the node started normally and served RPC on [::1] only, and nothing
in debug.log said so -- the failure text was kept for the "nothing bound" error
that never came. A client on 127.0.0.1, which is every default client and this
framework, was refused while the node looked healthy.

The test holds 127.0.0.1 on the node's RPC port itself, so the IPv4 bind fails
deterministically, and talks to the node over [::1]. It asserts the node says
what it lost, and that it is still serving over IPv6.
"""

import os
import socket
import sys

from test_framework.test_framework import GridcoinTestFramework, SkipTest
from test_framework.util import assert_equal, chain_subdir, rpc_port


def ipv6_loopback_usable():
    if not socket.has_ipv6:
        return False
    try:
        with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as s:
            s.bind(('::1', 0))
        return True
    except OSError:
        return False


class RPCBindIPv4ConflictTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-connect=0", "-listen=0"]]

    def setup_network(self):
        # The node is added here and started in run_test, once the port is held.
        # It is reached over [::1], the only listener it will have.
        self.add_nodes(self.num_nodes, self.extra_args, rpchost='[::1]')

    def run_test(self):
        if sys.platform == 'win32':
            # SO_REUSEADDR lets a second bind share the port on Windows, so the
            # conflict this test needs cannot be set up there.
            raise SkipTest("the IPv4 port conflict cannot be forced on Windows")
        if not ipv6_loopback_usable():
            raise SkipTest("no usable IPv6 loopback")

        node = self.nodes[0]
        port = rpc_port(0)

        # assert_debug_log reads from the file's current end, so it must exist
        # before the node's first start.
        chain_dir = os.path.join(node.datadir, chain_subdir(node.chain))
        os.makedirs(chain_dir, exist_ok=True)
        open(os.path.join(chain_dir, 'debug.log'), 'a', encoding='utf-8').close()

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as blocker:
            blocker.bind(('127.0.0.1', port))
            blocker.listen(1)

            self.log.info("127.0.0.1:%d is taken: the node must say it serves IPv6 only", port)
            with node.assert_debug_log(
                    expected_msgs=[
                        'RPC: bound and listening on [::1]:%d' % port,
                        'WARNING: StartRPCThreads: not listening on IPv4 127.0.0.1:%d' % port,
                        'RPC is reachable over IPv6 only',
                        'RPC server started: 1 acceptor(s)',
                    ],
                    unexpected_msgs=['bound and listening on 127.0.0.1:']):
                self.start_node(0)

            self.log.info("and it still serves RPC over [::1]")
            assert_equal(node.getblockcount(), 0)


if __name__ == '__main__':
    RPCBindIPv4ConflictTest().main()
