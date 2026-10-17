#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Startup coherence recovery resets the poll registry.

A poll's record is stored when its block connects. If the block's data is then lost -- here the block file is cut
short at the tip, as a crash can leave it -- startup coherence recovery rewinds below the block, and the record
goes with it: the poll, mined again, registers from its new block.

The startup contract replay never runs on regtest (see feature_poll_registry.py), so after a recovery here no poll
from the blocks below the rewind comes back either; on mainnet and testnet the replay rebuilds them from V11 in the
same start. This test keeps its poll in the tip block for that reason.
"""

import os

from test_framework.messages import MAGIC_BYTES, hash256
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, chain_subdir

STAKE_TIMESTAMP_MASK = 15
SPACING = 96
WEIGHT_BALANCE = 1
RESPONSE_SINGLE_CHOICE = 2
FIRST_BLOCK_FILE = "blk0001.dat"
EXTRA_ARGS = ["-staking=0", "-connect=0", "-listen=0", "-debug=vote"]


def block_record_start(blk_path, magic, block_hash):
    """The offset of the named block's record (its magic) in a blk????.dat file.

    A record is the 4-byte network magic, a 4-byte little-endian length, then the serialized block
    (src/node/blockstorage.cpp, WriteBlockToDisk); see feature_blockindex_verification.py.
    """
    with open(blk_path, "rb") as blk_file:
        data = blk_file.read()

    pos = 0

    while pos + 8 <= len(data) and data[pos:pos + 4] == magic:
        size = int.from_bytes(data[pos + 4:pos + 8], "little")

        # The block id hashes the header alone: 80 bytes, or 76 from block version 11, which drops the nonce (as
        # CBlockHeader.serialize in test_framework/messages.py). The framework cannot deserialize a block that carries a
        # poll contract, so the header is hashed here directly.
        version = int.from_bytes(data[pos + 8:pos + 12], "little", signed=True)
        header_size = 80 if version <= 10 else 76
        if hash256(data[pos + 8:pos + 8 + header_size])[::-1].hex() == block_hash:
            return pos

        pos += 8 + size

    raise AssertionError(f"no block record for {block_hash} in {blk_path}")


class PollCoherenceResetTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [list(EXTRA_ARGS)]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def stake(self):
        node = self.nodes[0]
        tip_time = node.getblock(node.getbestblockhash())["time"]
        node.advance_mocktime_to((tip_time + SPACING) & ~STAKE_TIMESTAMP_MASK)
        node.generatetoaddress(1, node.getnewaddress())

    def run_test(self):
        node = self.nodes[0]
        node.generatetoaddress(12, node.getnewaddress())
        self.grow_utxo_universe(node)

        poll_id = node.addpoll("survey", "Recovery", 7, "Which?", "A;B", WEIGHT_BALANCE, RESPONSE_SINGLE_CHOICE,
                               "https://gridcoin.us")["id"]
        raw_poll = node.getrawtransaction(poll_id)
        self.stake()
        tip_hash = node.getbestblockhash()
        tip_height = node.getblockcount()
        assert poll_id in node.getblock(tip_hash)["tx"]
        assert_equal(node.getpollresults(poll_id)["starting_block_height"], tip_height)

        self.log.info("the block file loses the tip block, as a crash can leave it; recovery rewinds below it")
        self.stop_node(0)
        blk_path = os.path.join(node.datadir, chain_subdir(node.chain), FIRST_BLOCK_FILE)
        start = block_record_start(blk_path, MAGIC_BYTES[node.chain], tip_hash)
        with open(blk_path, "r+b") as blk_file:
            blk_file.truncate(start)
        with node.assert_debug_log(["Phase 2 recovery complete"], timeout=60):
            self.start_node(0, list(EXTRA_ARGS))
        assert_equal(node.getblockcount(), tip_height - 1)

        self.log.info("the poll, mined again, registers from its new block")
        # The wallet may already have offered it back to the pool at startup.
        if poll_id not in node.getrawmempool():
            node.sendrawtransaction(raw_poll)
        self.stake()
        assert poll_id in node.getblock(node.getbestblockhash())["tx"]
        assert_equal(node.getpollresults(poll_id).get("starting_block_height"), tip_height)


if __name__ == "__main__":
    PollCoherenceResetTest().main()
