#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""CJDNS opt-in and onlynet startup coverage (bitcoin#23077).

Use a manual address without a CJDNS interface. Routine listeners and all
RPC/proxy sockets use loopback. One case checks the default IPv6 listener;
no remote outgoing connection is made.
"""
import socket
import struct
import threading

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

CJDNS = "fc00:1:2:3:4:5:6:7"


class CJDNSStartupTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        # Gridcoin treats -connect=0 as a literal destination when a proxy is set.
        # Zero automatic connection slots leave the manual onetry RPC available.
        self.common = ["-staking=0", "-walletbackupintervalsecs=0", "-maxconnections=0",
                       "-discover=0", "-bind=127.0.0.1", "-externalip=" + CJDNS]
        self.extra_args = [self.common]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def check(self, label, options, expected, *, default_bind=False):
        self.log.info("CJDNS startup %s", label)
        common = [a for a in self.common if not (default_bind and a.startswith("-bind="))]
        if default_bind:
            with self.nodes[0].assert_debug_log(["Bound to [::]:"],
                                               unexpected_msgs=["Bound to 0.0.0.0:"]):
                self.restart_node(0, common + options)
        else:
            self.restart_node(0, common + options)
        info = self.nodes[0].getnetworkinfo()
        advertised = [a["address"] for a in info["localaddresses"]]
        assert_equal(CJDNS in advertised, expected)
        assert_equal(info["connections"], 0)

    def check_proxy_classification(self):
        # Only CJDNS has a proxy in this configuration. A loopback server proves
        # fc00::/8 is classified before proxy choice; it never dials that address.
        observed = []
        errors = []
        complete = threading.Event()

        def read_exact(conn, size):
            data = b""
            while len(data) < size:
                part = conn.recv(size - len(data))
                assert part, "truncated SOCKS request"
                data += part
            return data
        with socket.socket() as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind(("127.0.0.1", 0))
            server.listen(1)
            server.settimeout(30 * self.options.timeout_factor)

            def serve():
                try:
                    with server.accept()[0] as conn:
                        conn.settimeout(10 * self.options.timeout_factor)
                        assert_equal(read_exact(conn, 3), b"\x05\x01\x00")
                        conn.sendall(b"\x05\x00")
                        assert_equal(read_exact(conn, 4), b"\x05\x01\x00\x03")
                        length = read_exact(conn, 1)[0]
                        host = read_exact(conn, length).decode("ascii")
                        port = struct.unpack(">H", read_exact(conn, 2))[0]
                        observed.append((host, port))
                        # Reject locally: never open an outgoing CJDNS connection.
                        conn.sendall(b"\x05\x01\x00\x01" + b"\x00" * 6)
                except Exception as e:
                    errors.append(e)
                finally:
                    complete.set()
            thread = threading.Thread(target=serve, daemon=True)
            thread.start()
            try:
                proxy = "127.0.0.1:%d" % server.getsockname()[1]
                self.restart_node(0, self.common + ["-cjdnsreachable",
                                                   "-onlynet=cjdns", "-proxy=" + proxy])
                assert_raises_rpc_error(-23, "Node connection failed",
                                        self.nodes[0].addnode, "[" + CJDNS + "]:32749", "onetry")
                self.wait_until(complete.is_set, timeout=35)
                if errors:
                    raise errors[0]
                assert_equal(observed, [(CJDNS, 32749)])
                self.log.info("CJDNS named-connection proxy classification passed")
            finally:
                thread.join(timeout=2)

    def run_test(self):
        initial = self.nodes[0].getnetworkinfo()
        assert CJDNS not in [a["address"] for a in initial["localaddresses"]]
        assert_equal(initial["connections"], 0)
        self.log.info("CJDNS startup default-off passed")
        self.check("explicit disabled", ["-cjdnsreachable=0"], False)
        self.check("reachable", ["-cjdnsreachable"], True)
        self.check("onlynet=cjdns reachable", ["-onlynet=cjdns", "-cjdnsreachable"], True)
        self.check("onlynet=cjdns default IPv6 listener",
                   ["-onlynet=cjdns", "-cjdnsreachable"], True, default_bind=True)
        self.check("onlynet=cjdns without opt-in", ["-onlynet=cjdns"], False)
        self.check("onlynet=ipv4 with opt-in", ["-onlynet=ipv4", "-cjdnsreachable"], False)
        self.check_proxy_classification()
        self.log.info("CJDNS startup and default-off checks passed (bitcoin#23077)")


if __name__ == "__main__":
    CJDNSStartupTest().main()
