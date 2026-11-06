#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""An interrupted -reindex keeps the original block files, and a later -reindex
finishes it.

A -reindex start renames every blk*.dat to blk*.dat.orig, writes the genesis
block to a new blk0001.dat while the block index loads, and imports the .orig
files near the end of init. Two ways of stopping that start used to lose block
data:

- A stop after the rename and before the import left the .orig files and a
  blk0001.dat holding only genesis. The next -reindex renamed that blk0001.dat
  over blk0001.dat.orig, destroying the first block file, and never imported
  the other .orig files. -printblocktree, which exits right after the block
  index loads, leaves the same files a stop at that point does, so the test
  uses it to make the stop deterministic, twice in a row.
- A stop during the import of the last file counted as a complete import, so
  every .orig file was deleted with part of it unread. The test stops the node
  with SIGTERM once the import has started and checks from the log that the
  import was cut short.

After each interruption a plain -reindex must restore the whole chain, and only
then remove the .orig files.

The .orig files are removed only once every one was read to its end, so a block
record cut off at the end of the last file, which a crash while a block was
being written leaves, must not stop that: a third case reindexes a block file
with such a record appended.

A -resetblockchaindata start removes the block data, and the last case checks
that this includes the .orig files that a stop after the rename leaves, so that
a later -reindex does not import them.
"""

import os
import re
import sys
import time

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, chain_subdir

# Long enough that the import of the chain takes well over the time the test
# needs to see it start and send SIGTERM (about 1 s for 3000 blocks on a fast
# machine, against a 10 ms poll).
CHAIN_LENGTH = 3000
TARGET_SPACING = 90


class ReindexInterruptedTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def chain_dir(self):
        return os.path.join(self.nodes[0].datadir, chain_subdir(self.chain))

    def block_files(self):
        return sorted(f for f in os.listdir(self.chain_dir()) if re.fullmatch(r"blk\d+\.dat(\.orig)?", f))

    def orig_files(self):
        return [f for f in self.block_files() if f.endswith(".orig")]

    def log_size(self):
        return os.path.getsize(self.nodes[0].debug_log_path)

    def log_since(self, offset):
        with open(self.nodes[0].debug_log_path, encoding="utf-8", errors="replace") as f:
            f.seek(offset)
            return f.read()

    def mark_node_stopped(self):
        # Clear what is_node_stopped() clears on a clean exit, for a node this
        # test stopped on purpose without the stop RPC.
        node = self.nodes[0]
        assert node.process.poll() is not None
        node.running = False
        node.process = None
        node.rpc_connected = False
        node.rpc = None

    def assert_whole_chain(self, height, best):
        node = self.nodes[0]
        assert_equal(node.getblockcount(), height)
        assert_equal(node.getbestblockhash(), best)
        assert_equal(self.orig_files(), [])

    def run_test(self):
        node = self.nodes[0]
        # Arguments given to a start replace the node's extra_args, so each start
        # passes them again: on regtest a node stakes without peers, and a staked
        # block would move the chain under the checks.
        base = self.extra_args[0]

        # Mine one block per target spacing, starting far enough in the past
        # that the whole chain is older than the real clock. Blocks mined faster
        # raise the stake difficulty until the miner finds no kernel, and a
        # restarted node's clock is no longer mocked, so an import would refuse
        # a block more than 128 s ahead of it. A slot with no kernel is skipped.
        node.setmocktime(int(time.time()) - (CHAIN_LENGTH + 100) * TARGET_SPACING * 2)
        address = node.getnewaddress()
        while node.getblockcount() < CHAIN_LENGTH:
            tip_time = node.getblock(node.getbestblockhash())["time"]
            node.setmocktime(max(node.mock_now(), tip_time + TARGET_SPACING))
            try:
                node.generatetoaddress(1, address)
            except JSONRPCException as e:
                assert "no stake found" in e.error["message"], e.error
                node.setmocktime(node.mock_now() + TARGET_SPACING)
        assert_greater_than(int(time.time()), node.getblock(node.getbestblockhash())["time"])
        height = node.getblockcount()
        best = node.getbestblockhash()
        self.stop_node(0)

        self.log.info("A stop after the rename, twice, then -reindex restores the chain")
        for _ in range(2):
            node.assert_start_raises_init_error(base + ["-reindex", "-printblocktree"])
            assert_greater_than(len(self.orig_files()), 0)
        self.start_node(0, base + ["-reindex"])
        self.assert_whole_chain(height, best)
        self.stop_node(0)

        self.log.info("A block record cut off at the end of the file does not stop the reindex")
        with open(os.path.join(self.chain_dir(), "blk0001.dat"), "r+b") as f:
            # A record is the message start, the block size and the block. Copy
            # the first record's message start and size and the first half of
            # its 80-byte block header, so reading the block runs off the end.
            record_start = f.read(8 + 40)
            f.seek(0, os.SEEK_END)
            f.write(record_start)
        self.start_node(0, base + ["-reindex"])
        self.assert_whole_chain(height, best)
        self.stop_node(0)

        # terminate() is TerminateProcess on Windows, not a shutdown request.
        if sys.platform != "win32":
            self.log.info("A stop during the import keeps the .orig files, then -reindex restores the chain")
            offset = self.log_size()
            node.start(base + ["-reindex"])
            # Poll faster than wait_until does: the import is about a second long.
            deadline = time.time() + 120
            while "LoadExternalBlockFile: blocks/s:" not in self.log_since(offset):
                assert time.time() < deadline, "the import did not start"
                assert node.process.poll() is None, "the node exited before its import started"
                time.sleep(0.01)
            node.process.terminate()
            node.process.wait(timeout=120)
            self.mark_node_stopped()

            loaded = [int(n) for n in re.findall(r"Loaded (\d+) blocks from external file", self.log_since(offset))]
            assert_equal(len(loaded), 1)
            # The import must have been cut short for this case to test anything.
            assert loaded[0] < height, "the import finished before SIGTERM arrived; raise CHAIN_LENGTH"
            assert_greater_than(len(self.orig_files()), 0)

            self.start_node(0, base + ["-reindex"])
            self.assert_whole_chain(height, best)
            self.stop_node(0)

        self.log.info("-resetblockchaindata after a stop after the rename removes the .orig files too")
        node.assert_start_raises_init_error(base + ["-reindex", "-printblocktree"])
        assert_greater_than(len(self.orig_files()), 0)
        self.start_node(0, base + ["-resetblockchaindata"])
        assert_equal(node.getblockcount(), 0)
        assert_equal(self.orig_files(), [])


if __name__ == '__main__':
    ReindexInterruptedTest().main()
