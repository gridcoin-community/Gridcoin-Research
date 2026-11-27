#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A reorg keeps a pooled poll whose claimed outputs no block spends.

After a block, a pooled poll leaves the memory pool only when that block spends an output its claim names, or from
v15 when the block takes its title. A reorg that disconnects the block holding a claimed output's transaction returns
that transaction to the pool; the poll waits there beside it and is mined once the transaction is back in a block.

node0 holds the claimed coin and creates the poll. node2 stakes the branch that confirms the coin's transaction, and
node1 stakes a longer branch without it.
"""

from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

STAKE_TIMESTAMP_MASK = 15
SPACING = 96
CLAIM_DEPTH = 10
FEE = Decimal("0.01")
WEIGHT_BALANCE = 1
RESPONSE_SINGLE_CHOICE = 2
EXTRA_ARGS = ["-staking=0", "-debug=vote"]


class PollPoolReorgTest(GridcoinTestFramework):
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
        """Move every clock one block spacing on, to a stake slot."""
        target = (max(n.mock_now() for n in self.nodes) + SPACING) & ~STAKE_TIMESTAMP_MASK
        for n in self.nodes:
            n.advance_mocktime_to(target)

    def stake(self, node, peers):
        """Stake one block on node and wait until peers hold it."""
        self.advance_clocks()
        node.generatetoaddress(1, node.getnewaddress())
        self.sync_blocks(peers, wait=0.1)

    def send_raw(self, node, inputs, outputs):
        raw = node.createrawtransaction([{"txid": u["txid"], "vout": u["vout"]} for u in inputs], outputs)
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        return node.sendrawtransaction(signed["hex"])

    def take_premine_off_shared_key(self):
        """Give each node coins of its own (as feature_poll_title_rule.py does).

        init.cpp plants the premine key in every regtest wallet. node0 spends nine premine outputs to one coin of its
        own, fifteen of node1's (so node1 can stake a branch alone) and node2's, in a block it stakes with the tenth,
        then spends that block's coinstake output, which returns to the shared key, in a block node2 stakes."""
        node0, node1, node2 = self.nodes
        premine = [u for u in node0.listunspent(0) if u["confirmations"] > node0.getblockcount()]
        assert_equal(len(premine), 10)
        spent = premine[:9]
        outputs = {node0.getnewaddress(): Decimal("150000")}
        outputs.update({node1.getnewaddress(): Decimal("20000") for _ in range(15)})
        outputs.update({node2.getnewaddress(): Decimal("100000") for _ in range(3)})
        outputs[node2.getnewaddress()] = sum(u["amount"] for u in spent) - Decimal("750000") - FEE
        self.send_raw(node0, spent, outputs)
        self.sync_mempools(wait=0.1, flush_scheduler=False)
        self.stake(node0, self.nodes)

        mine = {u["address"] for u in node0.listunspent(0) if u["amount"] == Decimal("150000")}
        shared = [u for u in node0.listunspent(0) if u["address"] not in mine]
        assert shared
        self.send_raw(node0, shared, {node2.getnewaddress(): sum(u["amount"] for u in shared) - FEE})
        self.sync_mempools(wait=0.1, flush_scheduler=False)
        self.stake(node2, self.nodes)

    def run_test(self):
        node0, node1, node2 = self.nodes
        self.take_premine_off_shared_key()
        for _ in range(CLAIM_DEPTH):
            self.stake(node2, self.nodes)

        self.log.info("node1 leaves; node2 confirms node0's coin in a new transaction and buries it")
        self.disconnect_nodes(0, 1)
        coin = [u for u in node0.listunspent() if u["amount"] == Decimal("150000")]
        assert_equal(len(coin), 1)
        moved = self.send_raw(node0, coin, {node0.getnewaddress(): coin[0]["amount"] - FEE})
        self.sync_mempools([node0, node2], wait=0.1, flush_scheduler=False)
        for _ in range(CLAIM_DEPTH + 1):
            self.stake(node2, [node0, node2])
        assert node0.gettransaction(moved)["confirmations"] > CLAIM_DEPTH

        self.log.info("node0 pools a poll that claims the moved coin; node2 does not see it")
        self.disconnect_nodes(0, 2)
        poll = node0.addpoll("survey", "Title", 7, "Which?", "A;B", WEIGHT_BALANCE, RESPONSE_SINGLE_CHOICE,
                             "https://gridcoin.us")["id"]
        claimed = {o["txid"] for a in node0.getvotingclaim(poll)["balance_claim"] for o in a["outpoints"]}
        assert_equal(claimed, {moved})
        assert_equal(node0.getrawmempool(), [poll])

        self.log.info("node1's longer branch, without the moved coin, replaces node2's on node0")
        for _ in range(CLAIM_DEPTH + 4):
            self.stake(node1, [node1])
        self.connect_nodes(0, 1)
        self.advance_clocks()
        # A reconnect fetches no block; the next block node1 announces brings the ones before it too.
        self.stake(node1, [node0, node1])
        assert_equal(node0.gettransaction(moved)["confirmations"], 0)

        self.log.info("the poll waits in the pool beside the coin's transaction, and is mined once it is confirmed")
        assert_equal(set(node0.getrawmempool()), {moved, poll})
        assert not node0.gettransaction(poll).get("abandoned", False)

        # A peer admits the poll only once its claimed output's transaction is in a block. sendrawtransaction relays a
        # transaction the pool already holds.
        node0.sendrawtransaction(node0.getrawtransaction(moved))
        self.wait_until(lambda: moved in node1.getrawmempool())
        self.stake(node1, [node0, node1])
        assert node0.gettransaction(moved)["confirmations"] > 0
        assert_equal(node0.getrawmempool(), [poll])

        node0.sendrawtransaction(node0.getrawtransaction(poll))
        self.wait_until(lambda: poll in node1.getrawmempool())
        self.stake(node1, [node0, node1])
        assert node0.gettransaction(poll)["confirmations"] > 0
        assert_equal([p["id"] for p in node0.listpolls(True)], [poll])


if __name__ == "__main__":
    PollPoolReorgTest().main()
