#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Balance weight of a vote whose own transaction spends a claimed coin.

A vote transaction pays its burn fee from the voter's largest coin, which is also
in the vote's balance claim. Every vote returns its change to an input address
(not gated). From v15, for polls that start at or after the v15 height:

  - the tally follows the claimed coin into that return output, as it already
    follows a coin into its coinstake;
  - the same holds when a vote on another poll spends the coin;
  - the newest vote that reaches a coin carries its weight, so a re-vote takes
    the weight from the earlier vote.

node0 holds the regtest premine and creates the polls; node2 stakes the blocks
that confirm everyone's transactions, so node0's coins stay put long enough to
back a poll claim. node1 starts with exactly one coin. Weights are
compared against node1's unspent outputs, which after each confirmed vote are
exactly the coins its latest vote holds. The wallet claims only coins with at least
CLAIM_DEPTH confirmations (CWalletTx::IsConfirmed), so every coin is buried that
deep before a vote claims it.
"""

from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

STAKE_TIMESTAMP_MASK = 15
# Block spacing on the mock clock: close to the 90 s target, so the difficulty
# stays where node2's weight can meet it on every block.
SPACING = 96

V15_HEIGHT = 30
POLL_DAYS = 7
CLAIM_DEPTH = 10
# node1's single coin, which node1 also stakes. Regtest stays at the minimum
# difficulty, where a coin's chance of finding a kernel on a slot grows with its
# weight (80 per GRC); 5000 GRC finds one on only about two slots in five and the
# miner gives up after five, while 50000 GRC finds one on every slot.
FUNDING = Decimal("50000")
FEE = Decimal("0.01")

# addpoll arguments: the weight type is passed as PollWeightType - 1, so 1 is
# BALANCE; response type 2 is SINGLE_CHOICE. SURVEY is the one poll type that
# may weigh by balance alone, which keeps magnitude and superblocks out of it.
WEIGHT_BALANCE = 1
RESPONSE_SINGLE_CHOICE = 2


class VoteBalanceWeightTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.chain = "regtest"
        self.setup_clean_chain = True
        common = ["-staking=0", f"-blockv15height={V15_HEIGHT}", "-debug=vote"]
        self.extra_args = [common[:], common[:], common[:]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_all()

    def connect_all(self):
        self.connect_nodes(0, 1)
        self.connect_nodes(0, 2)

    def stake(self, node=None):
        """Stake one block SPACING seconds on (node2 unless told) and sync all nodes.

        Every pending transaction is stamped before the new slot, so the block
        carries them (see advance_to_next_stake_slot).
        """
        node = node or self.nodes[2]
        now = max(n.mock_now() for n in self.nodes)
        target = (now + SPACING) & ~STAKE_TIMESTAMP_MASK
        for n in self.nodes:
            n.advance_mocktime_to(target)
        node.generatetoaddress(1, node.getnewaddress())
        self.sync_blocks(wait=0.1)

    def confirm(self, node=None):
        """Carry the mempool into the next block."""
        self.sync_mempools(wait=0.1, flush_scheduler=False)
        self.stake(node)

    def bury(self):
        """Stake CLAIM_DEPTH blocks, so the newest coins can be claimed."""
        for _ in range(CLAIM_DEPTH):
            self.stake()

    def send_raw(self, node, inputs, outputs):
        raw = node.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]} for u in inputs], outputs)
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        return node.sendrawtransaction(signed["hex"])

    def take_premine_off_shared_key(self):
        """Take the premine off the key every regtest node holds.

        init.cpp plants the premine key in every regtest wallet, so node1 would
        otherwise own node0's coins and vote with them. node0 spends nine premine
        outputs to its own addresses and node2's in a block it stakes with the
        tenth, then spends that block's coinstake output, which returns to the
        shared key, in a block node2 stakes. The premine outputs are the ones with
        more confirmations than the chain height (see feature_reorg_conflicted.py).
        """
        node0, node1, node2 = self.nodes
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

    def add_poll(self, title):
        poll = self.nodes[0].addpoll("survey", title, POLL_DAYS, "Which?", "A;B",
                                     WEIGHT_BALANCE, RESPONSE_SINGLE_CHOICE,
                                     "https://gridcoin.us")
        self.confirm()
        return poll["id"]

    def vote(self, poll_id, choice):
        """node1 votes; returns the vote transaction, confirmed and buried."""
        txid = self.nodes[1].votebyid(poll_id, choice)["vote_txid"]
        self.confirm()
        self.bury()
        return self.nodes[1].getrawtransaction(txid, 1)

    def address_of(self, outpoint):
        prev = self.nodes[1].getrawtransaction(outpoint["txid"], 1)
        return prev["vout"][outpoint["vout"]]["scriptPubKey"]["addresses"][0]

    def holdings(self):
        return sum(u["amount"] for u in self.nodes[1].listunspent(0))

    def results(self, poll_id):
        return self.nodes[1].getpollresults(poll_id)

    def run_test(self):
        node0, node1, _ = self.nodes

        self.take_premine_off_shared_key()
        self.bury()

        self.log.info("node1 receives a single coin")
        node0.sendtoaddress(node1.getnewaddress(), FUNDING)
        self.confirm()
        assert_equal(len(node1.listunspent(0)), 1)

        self.bury()
        while node0.getblockcount() < V15_HEIGHT:
            self.stake()
        poll_a = self.add_poll("poll a")
        poll_b = self.add_poll("poll b")
        assert V15_HEIGHT <= self.results(poll_a)["starting_block_height"]

        self.log.info("the vote returns its change to the address of the coin it spends")
        tx = self.vote(poll_a, 0)
        assert_equal(len(tx["vin"]), 1)
        spent_address = self.address_of(tx["vin"][0])
        returned = [o for o in tx["vout"]
                    if spent_address in o["scriptPubKey"].get("addresses", [])]
        assert_equal(len(returned), 1)
        assert_equal(len(node1.listunspent(0)), 1)

        self.log.info("from v15: the claimed coin's weight follows into that return")
        held = self.holdings()
        assert_equal(returned[0]["value"], held)
        result = self.results(poll_a)
        assert_equal(result["total_weight"], held)
        assert_equal(result["invalid_votes"], 0)

        self.log.info("a re-vote replaces the earlier vote")
        self.vote(poll_a, 1)
        held = self.holdings()
        result = self.results(poll_a)
        assert_equal(result["total_weight"], held)
        assert_equal(result["responses"][0]["weight"], 0)
        assert_equal(result["responses"][1]["weight"], held)
        assert_equal((result["invalid_votes"], result["superseded_votes"]), (0, 1))

        self.log.info("a vote on another poll keeps the coin's weight on the first")
        self.vote(poll_b, 0)
        held = self.holdings()
        assert_equal(self.results(poll_b)["total_weight"], held)
        assert_equal(self.results(poll_a)["total_weight"], held)

        self.log.info("a stake, then a re-vote: the re-vote carries the staked coin")
        self.stake(node1)
        self.bury()
        self.vote(poll_b, 1)
        held = self.holdings()
        result = self.results(poll_b)
        assert_equal(result["total_weight"], held)
        assert_equal(result["responses"][1]["weight"], held)
        assert_equal((result["invalid_votes"], result["superseded_votes"]), (0, 1))
        assert_equal(self.results(poll_a)["total_weight"], held)


if __name__ == "__main__":
    VoteBalanceWeightTest().main()
