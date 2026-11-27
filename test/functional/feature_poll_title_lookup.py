#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Every active poll is listed, and a lookup by a title names every poll with that title key.

Below v15 a poll whose title key is taken is still registered by its txid. It is listed beside the poll that owns the
title, a result, detail or vote lookup by the title fails with both poll ids, and each poll answers by its own id.

node0 and node1 each create a poll from a coin only they hold; node2 stakes every block, so those coins stay put.
"""

from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

STAKE_TIMESTAMP_MASK = 15
SPACING = 96
CLAIM_DEPTH = 10
FEE = Decimal("0.01")
WEIGHT_BALANCE = 1
RESPONSE_SINGLE_CHOICE = 2
EXTRA_ARGS = ["-staking=0", "-debug=vote"]


class PollTitleLookupTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [list(EXTRA_ARGS) for _ in range(self.num_nodes)]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_nodes(0, 1)
        self.connect_nodes(0, 2)

    def advance_clocks(self):
        """Move every clock onto the next stake slot; also clears the inbound rate limit a reconnect meets."""
        target = (max(n.mock_now() for n in self.nodes) + SPACING) & ~STAKE_TIMESTAMP_MASK
        for n in self.nodes:
            n.advance_mocktime_to(target)

    def stake(self, node=None):
        """Stake one block on (node2 unless told) and sync every node (as feature_vote_balance_weight.py)."""
        node = node or self.nodes[2]
        self.advance_clocks()
        node.generatetoaddress(1, node.getnewaddress())
        self.sync_blocks(wait=0.1)

    def confirm(self, node=None):
        """Carry the mempool into the next block."""
        self.sync_mempools(wait=0.1, flush_scheduler=False)
        self.stake(node)

    def send_raw(self, node, inputs, outputs):
        raw = node.createrawtransaction([{"txid": u["txid"], "vout": u["vout"]} for u in inputs], outputs)
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        return node.sendrawtransaction(signed["hex"])

    def take_premine_off_shared_key(self):
        """Take the premine off the key every regtest node holds (as feature_poll_title_rule.py)."""
        node0, node1, node2 = self.nodes
        premine = [u for u in node0.listunspent(0) if u["confirmations"] > node0.getblockcount()]
        assert_equal(len(premine), 10)
        spent = premine[:9]
        node1_address = node1.getnewaddress()
        outputs = {node0.getnewaddress(): Decimal("150000"), node1_address: Decimal("150000")}
        outputs.update({node2.getnewaddress(): Decimal("150000") for _ in range(3)})
        outputs[node2.getnewaddress()] = sum(u["amount"] for u in spent) - Decimal("750000") - FEE
        self.send_raw(node0, spent, outputs)
        self.confirm(node0)

        shared = [u for u in node1.listunspent(0) if u["address"] != node1_address]
        assert shared
        self.send_raw(node0, shared, {node0.getnewaddress(): sum(u["amount"] for u in shared) - FEE})
        self.confirm()
        assert_equal([(u["address"], u["amount"]) for u in node1.listunspent(0)], [(node1_address, Decimal("150000"))])

    def add_poll(self, node, title):
        return node.addpoll("survey", title, 7, "Which?", "A;B", WEIGHT_BALANCE, RESPONSE_SINGLE_CHOICE,
                            "https://gridcoin.us")["id"]

    def check(self, node, poll_a, poll_b):
        ids = f"2 polls have that title. Use the poll id: {poll_a}, {poll_b}"
        assert_equal(sorted(p["id"] for p in node.listpolls(True)), sorted([poll_a, poll_b]))
        assert_equal(node.getpollresults(poll_a)["poll_id"], poll_a)
        assert_equal(node.getpollresults(poll_b)["poll_id"], poll_b)
        assert_raises_rpc_error(-8, ids, node.getpollresults, "dup")
        assert_raises_rpc_error(-8, ids, node.votedetails, "dup")
        assert_raises_rpc_error(-8, ids, node.vote, "dup", "A")

    def run_test(self):
        node0, node1, node2 = self.nodes
        self.take_premine_off_shared_key()
        for _ in range(CLAIM_DEPTH):
            self.stake()

        self.log.info("below v15 a second poll with a taken title registers by txid")
        self.disconnect_nodes(0, 1)
        poll_a = self.add_poll(node0, "Dup")
        poll_b = self.add_poll(node1, "DUP")
        raw_b = node1.getrawtransaction(poll_b)
        self.sync_mempools([node0, node2], wait=0.1, flush_scheduler=False)
        self.advance_clocks()
        self.connect_nodes(0, 1)
        self.stake()
        assert poll_a in node2.getblock(node2.getbestblockhash())["tx"]
        node0.sendrawtransaction(raw_b)
        self.sync_mempools([node0, node2], wait=0.1, flush_scheduler=False)
        self.stake()
        assert poll_b in node2.getblock(node2.getbestblockhash())["tx"]

        self.log.info("both are listed; a lookup by the title names both; each answers by its id")
        for node in self.nodes:
            self.check(node, poll_a, poll_b)

        self.log.info("a restart gives the same")
        self.restart_node(0, extra_args=list(EXTRA_ARGS))
        self.check(node0, poll_a, poll_b)


if __name__ == "__main__":
    PollTitleLookupTest().main()
