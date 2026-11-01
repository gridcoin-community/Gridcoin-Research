#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Cross the v15 height on regtest and check the block spacing rules.

Two connected nodes, v15 at height H, mock clocks set explicitly per block:

- Below H every nBits follows the old linear retarget, and from H every nBits
  follows the log-linear retarget on the signed parent spacing. Both are
  recomputed here from the chain's own (time, bits) pairs.
- After a gap of more than 300 s the kernel target is eased: the peer that
  validates the block logs the eased target, and the block is accepted.
- A node whose clock is 60 s behind the tip stamps parent - 48 and its block is
  accepted; 200 s behind, it refuses to build a block its own window rejects.
- A block 64 s ahead of a peer's clock is rejected by that peer without
  penalty, and the peer asks for it again by itself once its clock is within
  48 s of the block's time.
- getclockstats counts received blocks by lead, and not the node's own blocks.
- A staking node whose clock is too far behind the tip to stake, with no time
  quorum, warns in the RPC errors field and in its log.
"""

import time

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

V15_HEIGHT = 30
SLOT = 16

# Reference implementations of both retarget rules, written from the
# specification (the v15 part mirrors the independent reference used to
# generate the unit test vectors).
LIMIT = ((1 << 256) - 1) >> 20


def set_compact(c):
    size, word = c >> 24, c & 0x007fffff
    return word >> (8 * (3 - size)) if size <= 3 else word << (8 * (size - 3))


def get_compact(v):
    size = (v.bit_length() + 7) // 8
    word = v << (8 * (3 - size)) if size <= 3 else v >> (8 * (size - 3))
    if word & 0x00800000:
        word >>= 8
        size += 1
    return (size << 24) | word


def scale_exp2(target, exp_q16):
    target = min(target, LIMIT)
    shifts = exp_q16 >> 16
    frac = exp_q16 - (shifts << 16)
    factor = 65536 + ((195766423245049 * frac + 971821376 * frac * frac + 5127 * frac ** 3 + (1 << 47)) >> 48)
    nxt = target * factor
    shifts -= 16
    nxt = nxt >> -shifts if shifts <= 0 else nxt << shifts
    return min(max(nxt, 1), LIMIT)


def retarget_v15(parent_bits, spacing):
    exp_q16 = ((min(spacing, 2500) - 90) * 2 * 65536 * 1000000) // (990 * 693147)
    return get_compact(scale_exp2(set_compact(parent_bits), exp_q16))


def retarget_old(parent_bits, spacing):
    target_spacing = 90
    interval = 960 // target_spacing
    t = spacing if spacing >= 0 else target_spacing
    new = set_compact(parent_bits) * ((interval - 1) * target_spacing + 2 * t) // ((interval + 1) * target_spacing)
    if new <= 0 or new > LIMIT:
        new = LIMIT
    return get_compact(new)


class FeatureV15SpacingTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        args = ["-staking=0", "-blockv15height=%d" % V15_HEIGHT, "-debug=verbose"]
        self.extra_args = [list(args), list(args)]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_nodes(1, 0)

    def tip(self, node):
        return node.getblock(node.getbestblockhash())

    def set_clock(self, node, t):
        node.setmocktime(t)

    def mine_after(self, spacing):
        """Set both clocks to the tip time plus `spacing`, mine one block on
        node0 and wait for node1 to have it. Returns the new tip."""
        node0, node1 = self.nodes
        t = self.tip(node0)["time"] + spacing
        self.set_clock(node0, t)
        self.set_clock(node1, t)
        node0.generatetoaddress(1, node0.getnewaddress())
        self.sync_blocks()
        return self.tip(node0)

    def run_test(self):
        node0, node1 = self.nodes
        addr = node0.getnewaddress()

        t0 = int(time.time()) // SLOT * SLOT + SLOT
        self.set_clock(node0, t0)
        self.set_clock(node1, t0)

        self.log.info("Mine to H - 1 under the old rule, keeping the target near the limit")
        node0.generatetoaddress(1, addr)
        self.sync_blocks()
        spacings = [32, 160]
        while node0.getblockcount() < 12:
            self.mine_after(spacings[node0.getblockcount() % 2])
        self.grow_utxo_universe(node0, count=48, agree_with=node1)
        while node0.getblockcount() < V15_HEIGHT - 1:
            self.mine_after(spacings[node0.getblockcount() % 2])

        self.log.info("Cross H with parent spacings of 16 s and 160 s")
        for spacing in (16, 160, 16):
            self.mine_after(spacing)
        assert node0.getblockcount() > V15_HEIGHT

        self.log.info("After a 400 s gap the validating peer eases the kernel target")
        with node1.assert_debug_log(["CheckProofOfStakeV8: eased kernel target"]):
            self.mine_after(400)

        self.log.info("60 s behind the tip: the stamp is lifted to at most 48 s before the parent")
        parent = self.tip(node0)
        self.set_clock(node0, parent["time"] - 60)
        self.set_clock(node1, parent["time"])
        node0.generatetoaddress(1, addr)
        self.sync_blocks()
        child = self.tip(node1)
        assert_equal(child["height"], parent["height"] + 1)
        assert -48 <= child["time"] - parent["time"] <= -16, child["time"] - parent["time"]

        self.log.info("200 s behind the tip: no block is built")
        parent = self.tip(node0)
        self.set_clock(node0, parent["time"] - 200)
        assert_raises_rpc_error(-32603, "acceptance window", node0.generatetoaddress, 1, addr)
        assert_equal(node0.getbestblockhash(), parent["hash"])

        # 64 s ahead on the grid keeps every stamp generatetoaddress can use (leads of 64-112 s)
        # inside the penalty-free band (48, 128].
        self.log.info("A block 64 s ahead of node1 is rejected without penalty, then fetched again")
        now = (max(parent["time"], node1.mock_now()) // SLOT + 1) * SLOT
        self.set_clock(node1, now)
        self.set_clock(node0, now + 64)
        leads_before = node1.getclockstats()
        node0.generatetoaddress(1, addr)
        ahead = self.tip(node0)
        self.wait_until(lambda: node1.getclockstats()["lead_over_48"] > leads_before["lead_over_48"])
        assert_equal(node1.getbestblockhash(), parent["hash"])
        assert_equal(node1.listbanned(), [])
        assert_equal(node1.getconnectioncount(), 1)
        # No child block and no other peer: node1 must ask for the block again itself once now + 48 >= its time.
        self.set_clock(node1, ahead["time"] - 48)
        self.wait_until(lambda: node1.getbestblockhash() == ahead["hash"], timeout=30)

        self.set_clock(node1, ahead["time"] + SLOT)
        self.set_clock(node0, ahead["time"] + SLOT)
        node0.generatetoaddress(1, addr)
        self.sync_blocks()
        assert_equal(node1.getblock(node1.getbestblockhash())["previousblockhash"], ahead["hash"])

        self.log.info("getclockstats counts received blocks, not the node's own")
        stats0 = node0.getclockstats()
        stats1 = node1.getclockstats()
        assert_equal(stats0["blocks_seen"], 0)
        assert_equal(stats1["time_quorum"], False)
        assert_equal(stats1["peer_median"], 0)
        assert stats1["blocks_seen"] >= node1.getblockcount(), stats1
        assert stats1["blocks_seen"] >= stats1["lead_over_16"] >= stats1["lead_over_32"] \
            >= stats1["lead_over_48"] >= stats1["lead_over_128"], stats1
        assert stats1["lead_over_48"] >= 1, stats1
        assert_equal(stats1["lead_over_128"], 0)

        self.log.info("Every nBits follows the rule in force at its height")
        blocks = [None] + [node0.getblock(node0.getblockhash(h)) for h in range(1, node0.getblockcount() + 1)]
        limit_bits = get_compact(LIMIT)
        assert_equal(int(blocks[1]["bits"], 16), limit_bits)
        assert_equal(int(blocks[2]["bits"], 16), limit_bits)
        for h in range(3, len(blocks)):
            parent_bits = int(blocks[h - 1]["bits"], 16)
            spacing = blocks[h - 1]["time"] - blocks[h - 2]["time"]
            rule = retarget_v15 if h >= V15_HEIGHT else retarget_old
            assert_equal((h, int(blocks[h]["bits"], 16)), (h, rule(parent_bits, spacing)))
            assert_equal(blocks[h]["version"] >= 15, h >= V15_HEIGHT)
        # The crossing saw negative parent spacing under the v15 rule.
        assert any(blocks[h - 1]["time"] < blocks[h - 2]["time"] for h in range(V15_HEIGHT, len(blocks)))

        self.log.info("A staking node too far behind the tip warns and logs it")
        # restart_node puts node0 back on the real clock, which the mocked chain tip is far ahead of, so the miner
        # skips every round. Regtest peers share one network group, so there is no time quorum either.
        lead = node0.getblock(node0.getbestblockhash())["time"] - int(time.time())
        assert lead > 2 * 48 + 60, "the tip must be well over 96 s ahead of the real clock for this check, got %d" % lead
        staking_args = [a for a in self.extra_args[0] if a != "-staking=0"] + ["-staking=1"]
        with node0.assert_debug_log(["Clock warning: staking without a network time quorum"], timeout=60):
            self.restart_node(0, extra_args=staking_args)
            self.wait_until(lambda: "time quorum" in node0.getnetworkinfo()["errors"], timeout=60)
        assert_equal(node0.getbestblockhash(), blocks[-1]["hash"])


if __name__ == '__main__':
    FeatureV15SpacingTest().main()
