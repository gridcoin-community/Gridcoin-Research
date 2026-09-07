#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""LoadBlockIndex's verification pass against a surgically corrupted block.

`CTxDB::LoadBlockIndex` (src/dbwrapper.cpp) re-verifies the trailing
`-checkblocks` window at every startup, and its failure arms have never had
dynamic coverage. The corruption anyone reaches for cannot arm them:

  * a truncated or otherwise unreadable file fails inside `ReadBlockFromDisk`,
    which the loop soft-fails out of, leaving the rest of the pass unrun;
  * damage to the header changes the block id, so `ReadBlockFromDisk`'s
    hash-versus-index check rejects it on that same path.

What arms the `CheckBlock` branch is a block that opens, deserializes cleanly,
and whose header still hashes to its index entry, while its transaction
payload no longer matches the merkle root the header carries. A random flip
essentially never lands there -- inside a transaction it breaks a varint and
the block stops deserializing, inside the header it breaks the id -- so this
test builds that block deliberately.

The mutation flips one bit of one txout `scriptPubKey`. A script is a
length-prefixed opaque byte string, so its length, every varint around it and
the serialized structure as a whole are untouched: the block still
deserializes and its id is unchanged. The transaction's hash changes, so the
merkle root no longer describes the transactions and `CheckBlock`'s merkle
comparison (src/validation.cpp) fails. All four of those properties are
asserted in python before the node restarts, so a mutation that stopped being
surgical fails here rather than quietly testing nothing.

The pass is exercised twice, because the two things worth pinning need the
damage in different places:

  * a mid-chain block shows the verification pass reporting the inconsistency,
    naming the height, and leaving the tip where it was;
  * the tip itself shows that the Phase 2 startup coherence recovery
    (`GRC::RunStartupCoherenceRecovery`, issue #2865) reads this exact block,
    finds it readable with a matching id, and declares the tip coherent. Its
    predicate is `read_ok && block.GetHash(true) == pindex->GetBlockHash()`
    (src/node/coherence.cpp), the same header-only test the read path applies,
    so this corruption class sits outside Phase 2's scope by construction.
    Pinning it keeps a future widening of Phase 2 a conscious decision.

A fourth restart reaches the one deeper arm a block-file mutation can arm.
Levels 2 to 4 sit inside `if (ReadTxIndex(hashTx, txindex))`, keyed on the
hash of the transaction as read from disk, so any payload mutation makes that
lookup miss and skips them. The level 5 arm is a sibling of that guard, not
nested under it, and it looks up `txin.prevout.hash` instead -- a fixed-width
field that a mutation can rewrite without disturbing a single varint. Pointing
a coinstake's input at an output the chain still holds unspent arms
`found unspent prevout`. That is why this arm is exercised at -checklevel=5
and by repointing an input, rather than at the -checklevel=4 with a vSpent
edit that issue #3322 suggests: the transaction-index records live in LevelDB,
which no python in this framework can write.

Nothing here is repaired by design: the verification pass reports and counts,
and the summary points at -reindex. The test asserts the tip is unmoved on
every restart, which is the property the removed legacy rewind used to
violate (issue #3315).
"""

import os
from io import BytesIO

from test_framework.messages import MAGIC_BYTES, CBlock
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, chain_subdir

# The first block file the node writes; OpenBlockFile rejects nFile < 1, and
# BlockFilePath formats it as blk%04u.dat directly in the chain's datadir.
FIRST_BLOCK_FILE = "blk0001.dat"

# Chain long enough that the corrupted mid-chain block is neither the tip nor
# genesis (the verification loop stops at a block with no parent), and short
# enough to stay inside the default -checkblocks window of 1000.
CHAIN_LENGTH = 12
MID_CHAIN_HEIGHT = 6

# A third block, kept clear of the other two so the deeper arm's report cannot
# be confused with theirs.
DEEP_CHECK_HEIGHT = 9


def read_block_records(blk_path, magic):
    """Yield (offset, raw) for each block record in a blk????.dat file.

    A record is the 4-byte network magic, a 4-byte little-endian length, then
    that many bytes of serialized block (src/node/blockstorage.cpp,
    WriteBlockToDisk). The offset returned is of the serialized block itself,
    which is what a CBlockIndex's nBlockPos points at: the writer takes its
    ftell after the magic and the length have been written.

    Iteration stops at the first position that does not start with the magic,
    which on a well-formed file is the end of the last record. Nothing
    preallocates or pads these files.
    """
    with open(blk_path, "rb") as blk_file:
        data = blk_file.read()

    pos = 0

    while pos + 8 <= len(data):
        if data[pos:pos + 4] != magic:
            break

        size = int.from_bytes(data[pos + 4:pos + 8], "little")
        start = pos + 8

        if start + size > len(data):
            break

        yield start, data[start:start + size]
        pos = start + size


def find_block_record(blk_path, magic, block_hash):
    """Return (offset, raw) of the record holding the named block."""
    for offset, raw in read_block_records(blk_path, magic):
        block = CBlock()
        block.deserialize(BytesIO(raw))
        block.rehash()

        if block.hash == block_hash:
            return offset, raw

    raise AssertionError(f"no block record for {block_hash} in {blk_path}")


def break_merkle_linkage(raw):
    """Flip one bit of one txout script, leaving the structure and id intact.

    Returns the mutated bytes. Every property the corruption class depends on
    is asserted here, so this cannot degrade into a no-op or into ordinary
    unreadable-file corruption without failing.
    """
    block = CBlock()
    block.deserialize(BytesIO(raw))

    # A faithful round trip is what makes the mutation below exact: whatever
    # comes out differs from the original in the one byte changed and nowhere
    # else.
    assert_equal(block.serialize(), raw)

    block.rehash()
    block_id = block.hash

    # Only a payout script may be touched, and the structural outputs must be
    # left exactly as they are. A PoS block's coinbase carries a single empty
    # output and its coinstake's vout[0] is the empty marker; IsCoinStake()
    # keys on that emptiness and IsProofOfStake() keys on IsCoinStake(). Break
    # either and the block reads as proof-of-work, which arms the PoW check
    # inside ReadBlockFromDisk itself: the read then fails, the verification
    # loop takes its soft-fail break without ever reaching CheckBlock, and
    # Phase 2 sees an unreadable block and really does rewind -- the opposite
    # of everything this test asserts. Skipping empty scripts avoids both by
    # construction; the assertions below say so out loud.
    target = None

    for tx_index, tx in enumerate(block.vtx):
        if tx_index == 0:
            continue

        for out_index, txout in enumerate(tx.vout):
            if len(txout.scriptPubKey) > 0:
                target = (tx_index, out_index)
                break

        if target is not None:
            break

    assert target is not None, "no non-empty scriptPubKey to corrupt"
    assert target[0] != 0, "the coinbase must keep its single empty output"
    assert target[1] != 0, "a coinstake's vout[0] must stay the empty marker"

    tx = block.vtx[target[0]]
    tx.rehash()
    original_txid = tx.hash

    script = bytearray(tx.vout[target[1]].scriptPubKey)
    script[-1] ^= 0x01
    tx.vout[target[1]].scriptPubKey = bytes(script)
    tx.sha256 = None
    tx.hash = None
    tx.rehash()

    # The merkle linkage is broken: the header still commits to the old hash.
    assert tx.hash != original_txid, "the flip did not change the transaction"

    mutated = block.serialize()

    # No length, no varint, no structure changed.
    assert_equal(len(mutated), len(raw))
    assert mutated != raw, "the flip did not change the serialized block"

    # It still deserializes, and its id is unchanged -- so ReadBlockFromDisk's
    # hash-versus-index check, and Phase 2's identical one, both still pass.
    after = CBlock()
    after.deserialize(BytesIO(mutated))
    after.rehash()
    assert_equal(after.hash, block_id)

    return mutated


def repoint_first_input(raw, txid, vout):
    """Aim a transaction's first input at a different outpoint.

    Returns (mutated bytes, the transaction's new hash). A prevout is a
    32-byte hash and a 4-byte index, both fixed width, so rewriting them
    disturbs no varint and the block still deserializes with its id intact --
    the same surgical property the script flip relies on, reached through a
    different field.
    """
    block = CBlock()
    block.deserialize(BytesIO(raw))
    assert_equal(block.serialize(), raw)

    block.rehash()
    block_id = block.hash

    # The coinstake, never the coinbase: a proof-of-stake coinbase carries one
    # empty output and nothing here should disturb it. Its input must stay
    # non-null for IsCoinStake(), which repointing preserves.
    assert len(block.vtx) >= 2, "expected a coinstake"
    tx = block.vtx[1]
    assert len(tx.vin) >= 1, "expected the coinstake to have an input"

    tx.rehash()
    original_txid = tx.hash

    tx.vin[0].prevout.hash = int(txid, 16)
    tx.vin[0].prevout.n = vout
    tx.sha256 = None
    tx.hash = None
    tx.rehash()

    assert tx.hash != original_txid, "repointing did not change the transaction"

    mutated = block.serialize()
    assert_equal(len(mutated), len(raw))

    after = CBlock()
    after.deserialize(BytesIO(mutated))
    after.rehash()
    assert_equal(after.hash, block_id)

    return mutated, tx.hash


class BlockIndexVerificationTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [[
            "-staking=0",
            "-connect=0",
            "-listen=0",
        ]]

    def setup_network(self):
        # Skip setup_nodes()'s deterministic-coinbase import: it calls
        # createwallet, which this single-wallet node does not serve, and the
        # node's own wallet is all this test needs.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def blk_path(self):
        node = self.nodes[0]
        return os.path.join(node.datadir, chain_subdir(node.chain), FIRST_BLOCK_FILE)

    def corrupt_block(self, block_hash, mutate=break_merkle_linkage):
        """Rewrite the named block in place, with the node stopped.

        Returns whatever the mutator returned beyond the bytes, so a caller
        can assert on the identity the node will name in its log.
        """
        blk_path = self.blk_path()
        magic = MAGIC_BYTES[self.nodes[0].chain]

        offset, raw = find_block_record(blk_path, magic, block_hash)
        result = mutate(raw)
        mutated, extra = result if isinstance(result, tuple) else (result, None)

        with open(blk_path, "r+b") as blk_file:
            blk_file.seek(offset)
            blk_file.write(mutated)

        return extra

    def run_test(self):
        node = self.nodes[0]

        node.generatetoaddress(CHAIN_LENGTH, node.getnewaddress())
        tip_height = node.getblockcount()
        tip_hash = node.getbestblockhash()
        assert_equal(tip_height, CHAIN_LENGTH)

        mid_hash = node.getblockhash(MID_CHAIN_HEIGHT)

        self.log.info("a clean chain verifies with no inconsistencies")
        with node.assert_debug_log(
                expected_msgs=["Verifying last"],
                unexpected_msgs=["found bad block at", "verification found"],
                timeout=60):
            self.restart_node(0)

        assert_equal(node.getblockcount(), tip_height)

        self.log.info("a mid-chain block whose payload no longer matches its "
                      "merkle root is reported, and the tip is left alone")
        self.stop_node(0)
        self.corrupt_block(mid_hash)

        with node.assert_debug_log(expected_msgs=[
                "CheckBlock: hashMerkleRoot mismatch",
                f"found bad block at {MID_CHAIN_HEIGHT}, hash={mid_hash}",
                "verification found 1 inconsistency",
                "start with -reindex if the chain does not recover",
        ], timeout=60):
            self.start_node(0)

        assert_equal(node.getblockcount(), tip_height)
        assert_equal(node.getbestblockhash(), tip_hash)

        self.log.info("the same corruption at the tip is still coherent to "
                      "the Phase 2 startup recovery, which leaves it alone")
        self.stop_node(0)
        self.corrupt_block(tip_hash)

        # The mid-chain block is still corrupt, so the count rising to two
        # also pins that a CheckBlock failure does not stop the pass: the loop
        # keeps scanning and counting, where the read-failure arm above it
        # deliberately breaks out.
        with node.assert_debug_log(
                expected_msgs=[
                    f"found bad block at {tip_height}, hash={tip_hash}",
                    f"found bad block at {MID_CHAIN_HEIGHT}, hash={mid_hash}",
                    "verification found 2 inconsistencies",
                    f"chain tip at height {tip_height} is coherent. No rewind needed.",
                ],
                unexpected_msgs=[
                    "detected inconsistency past height",
                    "failed coherence check",
                ],
                timeout=60):
            self.start_node(0)

        assert_equal(node.getblockcount(), tip_height)
        assert_equal(node.getbestblockhash(), tip_hash)

        self.log.info("at -checklevel=5 an input repointed at an unspent "
                      "output is reported by the deeper arm")
        # A confirmed wallet output: on chain, so the transaction index holds
        # it, and unspent, so its vSpent slot is null -- which is exactly the
        # condition the arm reports.
        unspent = [u for u in node.listunspent() if u["confirmations"] > 0]
        assert unspent, "expected a confirmed unspent output to point at"
        outpoint = unspent[0]

        deep_hash = node.getblockhash(DEEP_CHECK_HEIGHT)

        self.stop_node(0)
        repointed_txid = self.corrupt_block(
            deep_hash,
            lambda raw: repoint_first_input(raw, outpoint["txid"], outpoint["vout"]))

        with node.assert_debug_log(expected_msgs=[
                f"found unspent prevout {outpoint['txid']}:{outpoint['vout']} "
                f"in {repointed_txid}",
        ], timeout=60):
            self.start_node(0, self.extra_args[0] + ["-checklevel=5"])

        assert_equal(node.getblockcount(), tip_height)
        assert_equal(node.getbestblockhash(), tip_hash)


if __name__ == "__main__":
    BlockIndexVerificationTest().main()
