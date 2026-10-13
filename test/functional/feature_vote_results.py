#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than

STAKE_TIMESTAMP_MASK = 15
SPACING = 96
POLL_DAYS = 7
CLAIM_DEPTH = 10
FEE = Decimal("0.01")

WEIGHT_BALANCE = 1
RESPONSE_SINGLE_CHOICE = 2


class VoteResultsTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-debug=vote"] for _ in range(self.num_nodes)]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_all()

    def connect_all(self):
        for i in range(1, self.num_nodes):
            self.connect_nodes(0, i)

    def stake(self, node=None):
        node = node or self.nodes[2]
        now = max(n.mock_now() for n in self.nodes)
        target = (now + SPACING) & ~STAKE_TIMESTAMP_MASK
        for n in self.nodes:
            n.advance_mocktime_to(target)
        node.generatetoaddress(1, node.getnewaddress())
        self.sync_blocks(wait=0.1)

    def confirm(self, node=None):
        self.sync_mempools(wait=0.1, flush_scheduler=False)
        self.stake(node)

    def bury(self):
        for _ in range(CLAIM_DEPTH):
            self.stake()

    def warp(self, seconds):
        target = (max(node.mock_now() for node in self.nodes) + seconds) & ~STAKE_TIMESTAMP_MASK
        for node in self.nodes:
            node.setmocktime(target)
        self.wait_until(lambda: all(node.getconnectioncount() == 0 for node in self.nodes),
                        timeout=30)
        self.connect_all()

    def send_raw(self, node, inputs, outputs):
        raw = node.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]} for u in inputs], outputs)
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        return node.sendrawtransaction(signed["hex"])

    def take_premine_off_shared_key(self):
        node0, node1, node2 = self.nodes[:3]
        premine = [u for u in node0.listunspent(0) if u["confirmations"] > node0.getblockcount()]
        assert_equal(len(premine), 10)
        spent = premine[:9]
        outputs = {node0.getnewaddress(): Decimal("150000") for _ in range(2)}
        outputs.update({node2.getnewaddress(): Decimal("150000") for _ in range(3)})
        outputs[node2.getnewaddress()] = sum(u["amount"] for u in spent) - Decimal("750000") - FEE
        self.send_raw(node0, spent, outputs)
        self.confirm(node0)

        shared = node1.listunspent(0)
        assert_equal(len(shared), 1)
        self.send_raw(node0, shared, {node0.getnewaddress(): shared[0]["amount"] - FEE})
        self.confirm()
        assert_equal(node1.listunspent(0), [])
        assert_equal(self.nodes[3].listunspent(0), [])

    def add_poll(self, title):
        poll = self.nodes[0].addpoll("survey", title, POLL_DAYS, "Which?", "A;B",
                                     WEIGHT_BALANCE, RESPONSE_SINGLE_CHOICE,
                                     "https://gridcoin.us")
        return poll["id"]

    def fund(self, node, amounts):
        for amount in amounts:
            self.nodes[0].sendtoaddress(node.getnewaddress(), amount)

    def vote(self, node, poll_id, choice):
        node.votebyid(poll_id, choice)
        self.confirm()
        self.bury()

    def spend_all(self, node):
        coins = node.listunspent(0)
        self.send_raw(node, coins, {self.nodes[0].getnewaddress(): sum(c["amount"] for c in coins) - FEE})

    def check(self, poll_id, weight):
        result = self.nodes[0].getpollresults(poll_id)
        assert_equal(result["votes"], 2)
        assert_equal(result["invalid_votes"], 1)
        assert_equal(result["total_weight"], weight)
        assert_equal([(r["choice"], r["weight"]) for r in result["responses"]], [("A", 0), ("B", weight)])
        return result

    def run_test(self):
        node0, node1, node2, node3 = self.nodes
        self.take_premine_off_shared_key()
        self.bury()

        first = self.add_poll("first")
        second = self.add_poll("second")
        self.fund(node1, [Decimal("50000"), Decimal("20000"), Decimal("10000")])
        self.fund(node3, [Decimal("50000"), Decimal("60000")])
        self.confirm()
        self.bury()

        self.vote(node1, first, 0)
        self.vote(node1, first, 1)
        self.check(first, Decimal("30000"))

        self.vote(node3, second, 0)
        utxos = node3.listunspent(0)
        assert_equal(len(utxos), 2)
        change = [c for c in utxos if c["amount"] != Decimal("50000")]
        assert_equal(len(change), 1)
        self.send_raw(node3, change, {node0.getnewaddress(): change[0]["amount"] - FEE})
        self.confirm()
        coin = node3.listunspent(0)
        assert_equal([c["amount"] for c in coin], [Decimal("50000")])
        self.stake(node3)
        staked = node3.listunspent(0)
        assert_equal(len(staked), 1)
        assert_equal(staked[0]["address"], coin[0]["address"])
        assert_greater_than(staked[0]["amount"], Decimal("50000"))
        self.fund(node3, [Decimal("60000")])
        self.confirm()
        self.bury()
        self.vote(node3, second, 1)
        self.check(second, staked[0]["amount"])

        self.warp((POLL_DAYS + 1) * 86400)
        self.stake()
        results = {poll_id: node0.getpollresults(poll_id) for poll_id in (first, second)}
        assert all(result["poll_expired"] for result in results.values())

        self.spend_all(node1)
        self.spend_all(node3)
        self.confirm()
        assert_equal(node1.listunspent(0), [])
        assert_equal(node3.listunspent(0), [])

        assert_equal(self.check(first, Decimal("30000")), results[first])
        assert_equal(self.check(second, staked[0]["amount"]), results[second])


if __name__ == "__main__":
    VoteResultsTest().main()
