#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A pooled poll meets the poll rules of the next block.

The memory pool judges a poll by the rules the next block will judge it by, at the next block's height, and refuses one
that block would refuse without penalizing the peer that relays it.

node0 and node2 activate multi-address poll claims at MULTI_ADDRESS_HEIGHT; node1 never does, so node1 builds a poll
with a single-address claim that node0's next block would refuse. node0 and node1 each hold a coin only they hold;
node2 stakes every block, so those coins stay put.
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
# The chain is at 2 after take_premine_off_shared_key and at 12 after CLAIM_DEPTH more blocks: the polls are built
# there, so the next block is the first with multi-address claims on node0 and node2.
MULTI_ADDRESS_HEIGHT = 13
EXTRA_ARGS = ["-staking=0", "-debug=vote", "-debug=contract"]


class PollNextBlockRulesTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [
            EXTRA_ARGS + [f"-pollmultiaddressheight={MULTI_ADDRESS_HEIGHT}"],
            EXTRA_ARGS + ["-pollmultiaddressheight=1000000"],
            EXTRA_ARGS + [f"-pollmultiaddressheight={MULTI_ADDRESS_HEIGHT}"],
        ]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_nodes(0, 1)
        self.connect_nodes(0, 2)

    def stake(self, node=None, nodes=None):
        """Stake one block SPACING seconds on (node2 unless told) and sync the nodes (as feature_poll_title_rule.py)."""
        node = node or self.nodes[2]
        nodes = nodes or self.nodes
        target = (max(n.mock_now() for n in self.nodes) + SPACING) & ~STAKE_TIMESTAMP_MASK
        for n in self.nodes:
            n.advance_mocktime_to(target)
        node.generatetoaddress(1, node.getnewaddress())
        self.sync_blocks(nodes, wait=0.1)

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
        """Take the premine off the key every regtest node holds (as feature_poll_title_rule.py).

        init.cpp plants the premine key in every regtest wallet, so the two polls could otherwise spend the same coin.
        node0 spends nine premine outputs to coins of its own, node1's and node2's in a block it stakes with the tenth,
        then spends that block's coinstake output, which returns to the shared key, in a block node2 stakes. The
        premine outputs are the ones with more confirmations than the chain height.
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
        assert_equal(node0.getblockcount(), MULTI_ADDRESS_HEIGHT - 1)

        self.log.info("the pool refuses a poll the next block would refuse, unpunished")
        with node0.assert_debug_log([], unexpected_msgs=["misbehaving"]):
            with node0.assert_debug_log(["rejected v1 poll claim after multi-address activation",
                                         "assigning DoS misbehavior of 0"], timeout=30):
                poll_a = self.add_poll(node1, "Title")
        assert_equal(node1.getrawmempool(), [poll_a])
        assert_equal(node0.getrawmempool(), [])
        assert_equal(node0.testmempoolaccept([node1.getrawtransaction(poll_a)])[0]["reject-reason"],
                     "invalid-contract")


if __name__ == "__main__":
    PollNextBlockRulesTest().main()
