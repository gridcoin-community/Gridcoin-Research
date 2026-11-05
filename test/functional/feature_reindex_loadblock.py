#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Conflicting block import options are refused at startup.

-reindex rebuilds the index from the block data files already on disk, and
-loadblock adds the blocks it imports to the current index, so asking for both
at once makes no sense. The two used to run one after the other in the same
start: the reindex imported the blk*.dat.orig files and deleted them, and the
-loadblock import then reused the reindex's file list, reopened the deleted
files and crashed.

A bootstrap.dat in the data directory is imported in place of either, and was
refused with them only at the import, after -reindex had renamed the block
data files.

The node must refuse each combination before -reindex renames any block data
file, and still start normally afterwards.
"""

import os
import re
import shutil

from test_framework.test_framework import GridcoinTestFramework
from test_framework.test_node import ErrorMatch
from test_framework.util import assert_equal, chain_subdir


class ReindexLoadblockTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def block_files(self):
        chain_dir = os.path.join(self.nodes[0].datadir, chain_subdir(self.chain))
        return sorted((f, os.path.getsize(os.path.join(chain_dir, f)))
                      for f in os.listdir(chain_dir) if re.match(r"blk\d+\.dat", f))

    def run_test(self):
        node = self.nodes[0]
        node.generatetoaddress(5, node.getnewaddress())
        height = node.getblockcount()
        best = node.getbestblockhash()
        self.stop_node(0)

        chain_dir = os.path.join(node.datadir, chain_subdir(self.chain))
        shutil.copy(os.path.join(chain_dir, "blk0001.dat"), os.path.join(chain_dir, "extra.dat"))
        files = self.block_files()

        self.log.info("-reindex with -loadblock is refused before any block data file is renamed")
        node.assert_start_raises_init_error(
            ["-reindex", "-loadblock=extra.dat"],
            "-reindex and -loadblock cannot be used together",
            match=ErrorMatch.PARTIAL_REGEX)
        assert_equal(self.block_files(), files)

        self.log.info("A bootstrap.dat with -reindex or -loadblock is refused before any block data file is renamed")
        bootstrap = os.path.join(chain_dir, "bootstrap.dat")
        shutil.copy(os.path.join(chain_dir, "blk0001.dat"), bootstrap)
        for args in (["-reindex"], ["-loadblock=extra.dat"]):
            node.assert_start_raises_init_error(
                args,
                "a bootstrap.dat file was found in the data directory",
                match=ErrorMatch.PARTIAL_REGEX)
            assert_equal(self.block_files(), files)
        os.remove(bootstrap)

        self.log.info("The node then starts normally with its chain")
        self.start_node(0)
        assert_equal(node.getblockcount(), height)
        assert_equal(node.getbestblockhash(), best)


if __name__ == '__main__':
    ReindexLoadblockTest().main()
