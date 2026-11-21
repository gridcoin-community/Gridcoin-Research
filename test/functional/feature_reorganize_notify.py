#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A forced reorganize runs -blocknotify for the block it leaves as the tip.

The developer `reorganize` RPC rolls the chain back to a given block, and then
does what a connected block does, -blocknotify included. One isolated node
mines three blocks, each of which is notified, and is reorganized back to the
first of them, whose hash must then be notified a second time.

Each notify command runs on its own thread, so the notices can land in any
order: the test counts how often a hash occurs and never reads the last line.
"""

import os
import sys

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

# The notify command: appends its first argument and a newline to the file its
# second argument names, in one write to a file opened for appending.
APPEND = [
    "import os, sys",
    "fd = os.open(sys.argv[2], os.O_WRONLY | os.O_APPEND | os.O_CREAT, 0o644)",
    "os.write(fd, (sys.argv[1] + chr(10)).encode())",
    "os.close(fd)",
]


class ReorganizeNotifyTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True

    def setup_network(self):
        notify_dir = os.path.join(self.options.tmpdir, "notify")
        os.mkdir(notify_dir)
        self.script = os.path.join(notify_dir, "append.py")
        self.notices = os.path.join(notify_dir, "notices.txt")
        with open(self.script, "w", encoding="utf8") as f:
            f.write(chr(10).join(APPEND) + chr(10))
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0",
                            "-blocknotify=%s %s %%s %s" % (sys.executable, self.script, self.notices)]]
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def notified(self):
        """The hashes -blocknotify has written so far, one per notice."""
        if not os.path.exists(self.notices):
            return []
        with open(self.notices, encoding="utf8") as f:
            return f.read().split()

    def run_test(self):
        node = self.nodes[0]

        self.log.info("mine three blocks: each one is notified once")
        target = node.generatetoaddress(3, node.getnewaddress())[0]
        self.wait_until(lambda: len(self.notified()) == 3, timeout=30)
        assert_equal(self.notified().count(target), 1)

        self.log.info("reorganize back to the first of them: it is notified again")
        assert_equal(node.reorganize(target)["RollbackChain"], True)
        self.wait_until(lambda: self.notified().count(target) == 2, timeout=30)


if __name__ == "__main__":
    ReorganizeNotifyTest().main()
