#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""One poll per title key.

A poll's title key is its title in lower case without edge white space. The wallet refuses a poll whose title key a
registered or pooled poll holds. The memory pool holds one poll per title key, and refuses a second without
penalizing the peer that relays it.

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
EXTRA_ARGS = ["-staking=0", "-debug=vote", "-blockv15height=1"]


class PollTitleRuleTest(GridcoinTestFramework):
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

    def stake(self, node=None):
        """Stake one block SPACING seconds on (node2 unless told) and sync every node (as feature_vote_balance_weight.py)."""
        node = node or self.nodes[2]
        target = (max(n.mock_now() for n in self.nodes) + SPACING) & ~STAKE_TIMESTAMP_MASK
        for n in self.nodes:
            n.advance_mocktime_to(target)
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
        """Take the premine off the key every regtest node holds (as feature_vote_balance_weight.py).

        init.cpp plants the premine key in every regtest wallet, so the two polls could otherwise spend the same coin
        and leave the pool as a double spend rather than over their title. node0 spends nine premine outputs to coins
        of its own, node1's and node2's in a block it stakes with the tenth, then spends that block's coinstake output,
        which returns to the shared key, in a block node2 stakes. The premine outputs are the ones with more
        confirmations than the chain height.
        """
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

    def run_test(self):
        node0, node1, node2 = self.nodes
        self.take_premine_off_shared_key()
        for _ in range(CLAIM_DEPTH):
            self.stake()

        self.log.info("two nodes each pool one of two polls with one title key; each refuses the other's, unpunished")
        self.disconnect_nodes(0, 1)
        poll_a = self.add_poll(node0, "Title")
        poll_b = self.add_poll(node1, "TITLE")
        raw_a = node0.getrawtransaction(poll_a)
        raw_b = node1.getrawtransaction(poll_b)
        inputs_a = {(i["txid"], i["vout"]) for i in node0.decoderawtransaction(raw_a)["vin"]}
        inputs_b = [(i["txid"], i["vout"]) for i in node1.decoderawtransaction(raw_b)["vin"]]
        assert not inputs_a.intersection(inputs_b)
        self.sync_mempools([node0, node2], wait=0.1, flush_scheduler=False)
        self.stake_slot_only()
        self.connect_nodes(0, 1)
        # A transaction the pool already holds is relayed again by sendrawtransaction (resendtx offers only wallet
        # transactions the pool does not hold). Every peer here is local, so a penalty would not move the ban score
        # (CNode::Misbehaving only logs it); the log is the check, read after the steps below have given it time.
        with node0.assert_debug_log([], unexpected_msgs=["misbehaving"]), \
             node1.assert_debug_log([], unexpected_msgs=["misbehaving"]):
            with node0.assert_debug_log([f"poll in tx {poll_b} has the title of poll {poll_a}"], timeout=30), \
                 node1.assert_debug_log([f"poll in tx {poll_a} has the title of poll {poll_b}"], timeout=30):
                node0.sendrawtransaction(raw_a)
                node1.sendrawtransaction(raw_b)
            assert_equal(node0.getrawmempool(), [poll_a])
            assert_equal(node1.getrawmempool(), [poll_b])
            assert_equal(node0.testmempoolaccept([raw_b])[0]["reject-reason"], "poll-duplicate-title")

            self.log.info("the wallet refuses a title a pooled poll holds")
            for node in (node0, node1):
                assert_raises_rpc_error(None, "Poll with that title already exists", self.add_poll, node, "title")


    def stake_slot_only(self):
        """Move every clock past the inbound rate limit a reconnect meets (see disconnect_nodes)."""
        target = (max(n.mock_now() for n in self.nodes) + SPACING) & ~STAKE_TIMESTAMP_MASK
        for n in self.nodes:
            n.advance_mocktime_to(target)


if __name__ == "__main__":
    PollTitleRuleTest().main()
