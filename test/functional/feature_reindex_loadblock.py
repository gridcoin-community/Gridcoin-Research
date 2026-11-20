#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Block imports are refused at startup where they conflict or would build on
the wrong index.

-reindex rebuilds the index from the block data files already on disk, and
-loadblock adds the blocks it imports to the current index, so asking for both
at once makes no sense. The two used to run one after the other in the same
start: the reindex imported the blk*.dat.orig files and deleted them, and the
-loadblock import then reused the reindex's file list, reopened the deleted
files and crashed. The node must refuse the combination before -reindex
renames any block data file, and still start normally afterwards.

A bootstrap.dat in the data directory is imported in place of either, and was
refused with them only at the import, after -reindex had renamed the block
data files. The node must refuse those combinations at the same point.

A chain can be imported in a series of -loadblock starts, each adding to the
index the ones before it built. The first of the series starts at the genesis
block, so it must start on an index that holds only genesis: on one that holds
more, the blocks already there stay in the index whether the import reaches
them or not. The node must refuse such a load and leave its block files alone,
-resetblockchaindata in the same start must let the load run, and a later load
that starts further up must add to the index. A bootstrap.dat that starts at
the genesis block must be refused and let run the same way.
"""

import os
import re
import shutil
import time

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import GridcoinTestFramework
from test_framework.test_node import ErrorMatch
from test_framework.util import assert_equal, assert_greater_than, chain_subdir

CHAIN_LENGTH = 8
# The first file of the series holds genesis to this height, the second the rest.
SPLIT_HEIGHT = 4
TARGET_SPACING = 90


class ReindexLoadblockTest(GridcoinTestFramework):
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
        return sorted((f, os.path.getsize(os.path.join(self.chain_dir(), f)))
                      for f in os.listdir(self.chain_dir()) if re.match(r"blk\d+\.dat", f))

    def block_records(self, hashes):
        # A record is the message start, the block size and the block. The
        # node wrote one record per block, in height order, which the block
        # headers' previous block hashes confirm.
        path = os.path.join(self.chain_dir(), "blk0001.dat")
        with open(path, "rb") as f:
            data = f.read()
        records = []
        pos = 0
        while pos < len(data):
            assert_equal(data[pos:pos + 4], data[0:4])
            size = int.from_bytes(data[pos + 4:pos + 8], "little")
            records.append(data[pos:pos + 8 + size])
            pos += 8 + size
        assert_equal(len(records), len(hashes))
        for height in range(1, len(records)):
            assert_equal(records[height][12:44][::-1].hex(), hashes[height - 1])
        return records

    def write_file(self, name, records):
        path = os.path.join(self.chain_dir(), name)
        with open(path, "wb") as f:
            f.write(b"".join(records))

    def run_test(self):
        node = self.nodes[0]
        # Arguments given to a start replace the node's extra_args, so each start
        # passes them again: on regtest a node stakes without peers, and a staked
        # block would move the chain under the checks.
        base = self.extra_args[0]

        # Mine one block per target spacing, starting in the past, so that the
        # chain is older than the real clock: a restarted node's clock is no
        # longer mocked, and an import refuses a block more than 128 s ahead of
        # it. A slot with no kernel is skipped.
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
        hashes = [node.getblockhash(height) for height in range(CHAIN_LENGTH + 1)]
        self.stop_node(0)

        records = self.block_records(hashes)
        self.write_file("first.dat", records[:SPLIT_HEIGHT + 1])
        self.write_file("second.dat", records[SPLIT_HEIGHT + 1:])
        files = self.block_files()

        self.log.info("-reindex with -loadblock is refused before any block data file is renamed")
        node.assert_start_raises_init_error(
            base + ["-reindex", "-loadblock=first.dat"],
            "-reindex and -loadblock cannot be used together",
            match=ErrorMatch.PARTIAL_REGEX)
        assert_equal(self.block_files(), files)

        self.log.info("A bootstrap.dat with -reindex or -loadblock is refused before any block data file is renamed")
        bootstrap = os.path.join(self.chain_dir(), "bootstrap.dat")
        shutil.copy(os.path.join(self.chain_dir(), "blk0001.dat"), bootstrap)
        for args in (["-reindex"], ["-loadblock=first.dat"]):
            node.assert_start_raises_init_error(
                base + args,
                "a bootstrap.dat file was found in the data directory",
                match=ErrorMatch.PARTIAL_REGEX)
            assert_equal(self.block_files(), files)
        os.remove(bootstrap)

        self.log.info("A -loadblock that starts at genesis is refused on an index that holds more than genesis")
        node.assert_start_raises_init_error(
            base + ["-loadblock=first.dat"],
            "starts at the genesis block",
            match=ErrorMatch.PARTIAL_REGEX)
        assert_equal(self.block_files(), files)

        self.log.info("A bootstrap.dat that starts at genesis is refused on an index that holds more than genesis")
        shutil.copy(os.path.join(self.chain_dir(), "first.dat"), bootstrap)
        node.assert_start_raises_init_error(
            base,
            "starts at the genesis block",
            match=ErrorMatch.PARTIAL_REGEX)
        assert_equal(self.block_files(), files)
        assert os.path.exists(bootstrap)
        os.remove(bootstrap)

        self.log.info("The node then starts normally with its chain")
        self.start_node(0)
        assert_equal(node.getblockcount(), CHAIN_LENGTH)
        assert_equal(node.getbestblockhash(), hashes[CHAIN_LENGTH])
        self.stop_node(0)

        self.log.info("-resetblockchaindata lets the first load of a series start at genesis")
        self.start_node(0, base + ["-resetblockchaindata", "-loadblock=first.dat"])
        assert_equal(node.getblockcount(), SPLIT_HEIGHT)
        assert_equal(node.getbestblockhash(), hashes[SPLIT_HEIGHT])
        self.stop_node(0)

        self.log.info("A later load that starts further up adds to the index")
        self.start_node(0, base + ["-loadblock=second.dat"])
        assert_equal(node.getblockcount(), CHAIN_LENGTH)
        assert_equal(node.getbestblockhash(), hashes[CHAIN_LENGTH])
        self.stop_node(0)

        self.log.info("-resetblockchaindata lets a bootstrap.dat that starts at genesis load")
        shutil.copy(os.path.join(self.chain_dir(), "first.dat"), bootstrap)
        self.start_node(0, base + ["-resetblockchaindata"])
        assert_equal(node.getblockcount(), SPLIT_HEIGHT)
        assert_equal(node.getbestblockhash(), hashes[SPLIT_HEIGHT])
        assert not os.path.exists(bootstrap)
        assert os.path.exists(bootstrap + ".old")


if __name__ == '__main__':
    ReindexLoadblockTest().main()
