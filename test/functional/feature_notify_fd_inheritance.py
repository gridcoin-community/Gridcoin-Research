#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Accepted RPC connections are not inherited by notify child processes.

A -blocknotify command is started while the framework's keep-alive RPC
connection is open. The child lists its own open descriptors; none may be the
listener or an accepted RPC connection of the node, since a child that holds
one keeps that connection (and the port) alive after the node closes it.

Linux only (reads /proc). A control first proves the method: a listener this
test opens and passes on explicitly must show up in the child's list.
"""

import os
import socket
import subprocess
import sys
import tempfile

from test_framework.test_framework import GridcoinTestFramework, SkipTest
from test_framework.util import rpc_port

LISTFDS = """\
import os, sys
out = sys.argv[2]
names = []
for fd in os.listdir('/proc/self/fd'):
    try:
        names.append(os.readlink('/proc/self/fd/' + fd))
    except OSError:
        pass
with open(os.path.join(out, 'fds-' + sys.argv[1] + '.tmp'), 'w', encoding='utf8') as f:
    f.write('\\n'.join(names))
os.rename(os.path.join(out, 'fds-' + sys.argv[1] + '.tmp'), os.path.join(out, 'fds-' + sys.argv[1] + '.txt'))
"""


def socket_inodes_for_port(port):
    """Inodes of every TCP socket (v4 and v6, any state) whose local port is `port`."""
    inodes = set()
    for table in ("/proc/net/tcp", "/proc/net/tcp6"):
        with open(table, encoding="utf8") as f:
            next(f)
            for line in f:
                fields = line.split()
                if int(fields[1].rsplit(":", 1)[1], 16) == port:
                    inodes.add(fields[9])
    return inodes


class NotifyFdInheritanceTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        if not sys.platform.startswith("linux"):
            raise SkipTest("reads /proc/<pid>/fd, Linux only")

    def setup_network(self):
        self.tmpdir_notify = tempfile.mkdtemp(prefix="notifyfd")
        self.script = os.path.join(self.tmpdir_notify, "listfds.py")
        with open(self.script, "w", encoding="utf8") as f:
            f.write(LISTFDS)
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0",
                            "-blocknotify=%s %s %%s %s" % (sys.executable, self.script, self.tmpdir_notify)]]
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def read_fds(self, tag):
        path = os.path.join(self.tmpdir_notify, "fds-%s.txt" % tag)
        self.wait_until(lambda: os.path.exists(path), timeout=30)
        with open(path, encoding="utf8") as f:
            return f.read().split("\n")

    def run_test(self):
        node = self.nodes[0]

        # Control. An explicitly passed listener must be visible to the method.
        sock = socket.socket()
        sock.bind(("127.0.0.1", 0))
        sock.listen(1)
        subprocess.run([sys.executable, self.script, "control", self.tmpdir_notify],
                       pass_fds=(sock.fileno(),), check=True)
        control_inodes = socket_inodes_for_port(sock.getsockname()[1])
        assert control_inodes
        names = self.read_fds("control")
        assert any("socket:[%s]" % i in names for i in control_inodes), "control: inherited socket not seen"
        sock.close()

        # The keep-alive RPC connection is open (the call below uses it).
        node.getblockcount()
        blockhash = node.generatetoaddress(1, node.getnewaddress())[0]
        names = self.read_fds(blockhash)
        rpc_inodes = socket_inodes_for_port(rpc_port(0))
        assert len(rpc_inodes) >= 2, "expected the listener and an accepted connection"
        leaked = [i for i in rpc_inodes if "socket:[%s]" % i in names]
        assert not leaked, "notify child inherited RPC sockets %s" % leaked
        self.log.info("notify child holds none of %d RPC sockets", len(rpc_inodes))


if __name__ == "__main__":
    NotifyFdInheritanceTest().main()
