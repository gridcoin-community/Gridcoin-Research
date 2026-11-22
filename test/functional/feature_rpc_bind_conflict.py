#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""An RPC address held by another program disables RPC; the node keeps running.

With neither -rpcbind nor -rpcallowip the daemon binds [::1] and 127.0.0.1 on the RPC
port. A loopback -rpcbind entry is treated the same way. When another program holds
one of those addresses the node must not exit and must not serve RPC on the other
family: it logs the conflict, raises a warning and runs on without RPC. An address the
host lacks (no IPv6) is not a conflict and is skipped. A held address that is not the
one the node would bind (127.0.0.2) changes nothing.

Skipped on Windows, where the framework cannot stop a node cleanly without RPC (see
feature_stop_node_without_rpc.py).
"""

import os
import socket
import sys
import time

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


def hold(host, port):
    family = socket.AF_INET6 if ':' in host else socket.AF_INET
    s = socket.socket(family, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if family == socket.AF_INET6:
        s.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
    s.bind((host, port))
    s.listen(1)
    return s


class RPCBindConflictTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        self.supports_cli = False

    def setup_network(self):
        # Nodes are added here and started per case, once the address is held.
        base = ["-connect=0", "-listen=0"]
        self.add_nodes(3, [base,
                           base + ["-rpcbind=127.0.0.1", "-rpcbind=[::1]"],
                           base])

    def wait_started(self, started):
        self.wait_until(lambda: os.path.exists(started), timeout=60)

    def read_log(self, node):
        path = os.path.join(node.datadir, chain_subdir(node.chain), 'debug.log')
        with open(path, encoding='utf-8', errors='replace') as f:
            return f.read()

    def case_disabled(self, name, idx, holder_host, probe_host):
        self.log.info("case %s: %s is held", name, holder_host)
        port = rpc_port(idx)
        node = self.nodes[idx]
        started = os.path.join(node.datadir, 'started')
        # The address in the message is the one that conflicted.
        shown = ('[%s]' % holder_host) if ':' in holder_host else holder_host
        with hold(holder_host, port):
            node.start(extra_args=["-startupnotify=touch %s" % started])
            self.wait_started(started)
            log = self.read_log(node)
            assert "RPC server disabled: %s:%d is in use by another program." % (shown, port) in log, log[-3000:]
            assert "ERROR: RPC: %s:%d is already in use by another program" % (shown, port) in log
            assert "RPC server started" not in log

            time.sleep(2)
            assert node.process.poll() is None, "the node exited"

            # The other loopback family is not served either.
            probe_family = socket.AF_INET6 if ':' in probe_host else socket.AF_INET
            with socket.socket(probe_family, socket.SOCK_STREAM) as c:
                c.settimeout(2)
                try:
                    c.connect((probe_host, port))
                    raise AssertionError("%s:%d accepted a connection" % (probe_host, port))
                except OSError:
                    pass

            # No RPC, so stop falls back to SIGTERM, which the node honours with a clean exit.
            self.assert_stop_without_rpc(node, shown, port)

    def assert_stop_without_rpc(self, node, shown, port):
        # Daemon mode prints the warning box text to stderr.
        expected_stderr = ("Warning: RPC server disabled: %s:%d is in use by another program. The node keeps "
                           "running without RPC. Stop that program, then restart Gridcoin to use RPC."
                           % (shown, port))
        try:
            node.stop_node(expected_stderr=expected_stderr)
            raise AssertionError("stop_node() should have had no RPC connection")
        except ConnectionError:
            pass
        node.wait_until_stopped()

    def run_test(self):
        if sys.platform == 'win32':
            raise SkipTest("the framework cannot stop a node without RPC cleanly on Windows")
        v6 = ipv6_loopback_usable()

        # The implicit path binds [::1] first, then 127.0.0.1. Hold 127.0.0.1.
        if v6:
            self.case_disabled('implicit_v4_taken', 0, '127.0.0.1', '::1')
        else:
            self.log.info("no usable IPv6 loopback: implicit_v4_taken probes only the held family")
            self.case_disabled('implicit_v4_taken', 0, '127.0.0.1', '127.0.0.2')

        if v6:
            self.case_disabled('explicit_loopback_taken', 1, '::1', '127.0.0.1')
        else:
            self.log.info("explicit_loopback_taken skipped: no usable IPv6 loopback")

        if sys.platform.startswith('linux'):
            self.log.info("case other_loopback_address_ignored: 127.0.0.2 is held")
            idx = 2
            port = rpc_port(idx)
            node = self.nodes[idx]
            with hold('127.0.0.2', port):
                self.start_node(idx)
                assert_equal(node.getblockcount(), 0)
                assert "RPC server disabled" not in self.read_log(node)
                self.stop_node(idx)


if __name__ == '__main__':
    RPCBindConflictTest().main()
