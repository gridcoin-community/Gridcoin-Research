#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A wallet transaction stops being conflicted when the block that conflicted it goes.

A connecting block that spends an input of the wallet's own pooled transaction
evicts it from the mempool, and the wallet is never told: the entry keeps its
in-mempool tag, and only the depth readers notice, reporting -1 because the pool
no longer holds it. Nothing records which block did it, so once that block is
reorganized away nothing can tell that the reason has gone. The transaction used
to stay at -1 for the rest of the session: gettransaction keeps reporting -1
confirmations and every depth-based reader renders it Conflicted, and it is not
in the local mempool for the next block to carry.

CWallet remembers which block displaced each conflicted transaction, in memory,
and the disconnect batch asks for its own hash back and re-offers what it gets to
the mempool -- after the resurrect loop, so a transaction that legitimately still
holds the outpoint gets it first.

The case builds the conflict out of a coinstake, which is the one conflicting
transaction that certainly does not come back: coinstakes are excluded from the
resurrect queue, so disconnecting the block frees the outpoint for good.

  * Two nodes start isolated. Both carry the regtest premine key, so both see
    the same ten premine outputs as their own.
  * Node 0 spends all ten into one transaction, which sits in its mempool. Its
    own staker will not touch those outputs now, but node 1 has never heard of
    the transaction and still will.
  * Node 1 stakes a block. The coinstake spends one of the ten.
  * The nodes connect, node 0 takes node 1's block, and the connect evicts its
    transaction as conflicted.
  * Node 1 is stopped and node 0 rolls the block back. The coinstake is gone
    for good, so the transaction is valid again -- and is pending again, with
    the outputs it spends withheld from listunspent.

On the unfixed daemon the last step fails: the mempool is empty and
gettransaction still reports -1.

Three more pairs of isolated nodes pin what that first case cannot reach:

  * The displacing transaction is a regular spend that the disconnect
    resurrects first, so the re-offer is refused and the transaction stays
    conflicted. A coinstake never comes back, so the first case cannot tell
    whether the re-offer runs before or after the resurrect loop; this one can.
  * The wallet had spent the displaced transaction's own output: the child is
    displaced with its parent and comes back with it, in that order.
  * The wallet had abandoned the displaced transaction: it is left alone.

The re-pooled transaction is also re-armed for announcement (getmempoolentry
reports it unbroadcast): every peer dropped it when the block connected, and
the wallet's own resend covers only what the pool does not hold.
"""
from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

FEE = Decimal("1.0")


class ReorgConflictedTest(GridcoinTestFramework):
    def set_test_params(self):
        self.chain = "regtest"
        self.setup_clean_chain = True
        # Four isolated pairs, one per case. Isolated: the conflict needs the
        # staker to stake in ignorance of the spender's transaction, so a pair
        # must not share a mempool until afterwards.
        self.num_nodes = 8
        self.extra_args = [["-staking=0", "-connect=0", "-listen=1"]] * 8

    def setup_network(self):
        # Bypass the base regtest createwallet path (Gridcoin has one default
        # BDB wallet, no multiwallet), and leave the nodes unconnected.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def premine_outputs(self, node):
        """The height-0 premine coinbase outputs.

        They carry one more confirmation than the chain height; every later
        output carries at most the height.
        """
        height = node.getblockcount()
        return [u for u in node.listunspent(0) if u["confirmations"] > height]

    def spend_all_premine(self, node, premine):
        total = sum(u["amount"] for u in premine)
        raw = node.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]} for u in premine],
            {node.getnewaddress(): total - FEE})
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        return node.sendrawtransaction(signed["hex"])

    def run_test(self):
        self.coinstake_displaces_and_goes_with_the_block(*self.nodes[0:2])
        self.regular_spend_displaces_and_comes_back(*self.nodes[2:4])
        self.descendant_follows_its_parent_back(*self.nodes[4:6])
        self.abandoned_transaction_is_left_alone(*self.nodes[6:8])

    def coinstake_displaces_and_goes_with_the_block(self, spender, staker):
        premine = self.premine_outputs(spender)
        assert premine, "no premine output to spend"
        self.log.info("both nodes hold the same %d premine outputs", len(premine))
        assert_equal(len(self.premine_outputs(staker)), len(premine))

        self.log.info("node 0 spends every premine output into one transaction")
        txid = self.spend_all_premine(spender, premine)
        assert_equal(spender.getrawmempool(), [txid])
        assert_equal(spender.gettransaction(txid)["confirmations"], 0)

        self.log.info("node 1, which has not seen it, stakes a premine output")
        self.advance_to_next_stake_slot(nodes=[spender, staker])
        staker.generatetoaddress(1, staker.getnewaddress())
        block_hash = staker.getbestblockhash()
        assert_equal(staker.getblockcount(), 1)

        self.log.info("node 0 takes that block: its transaction is conflicted")
        genesis_hash = spender.getbestblockhash()
        assert_equal(spender.getblockcount(), 0)
        self.connect_nodes(0, 1)
        self.sync_blocks([spender, staker])
        assert_equal(spender.getbestblockhash(), block_hash)
        assert_equal(spender.getrawmempool(), [])
        assert_equal(spender.gettransaction(txid)["confirmations"], -1)

        # Stop the staker so it cannot re-announce its branch over the rollback.
        # (Later cases hand their own pair to advance_to_next_stake_slot, which
        # otherwise touches every node, this stopped one included.)
        self.stop_node(1)

        self.log.info("roll the block back: the conflict is gone, so the transaction is pending again")
        assert_equal(spender.reorganize(genesis_hash)["RollbackChain"], True)
        assert_equal(spender.getbestblockhash(), genesis_hash)
        assert_equal(spender.getrawmempool(), [txid])
        assert_equal(spender.gettransaction(txid)["confirmations"], 0)
        # Every peer dropped it when the block connected, and the wallet's own
        # resend covers only what the pool does not hold, so the re-offer has
        # to re-arm the announcement itself.
        assert_equal(spender.getmempoolentry(txid)["unbroadcast"], True)

        unspent = {(u["txid"], u["vout"]) for u in spender.listunspent(0)}
        for u in premine:
            assert (u["txid"], u["vout"]) not in unspent

    def regular_spend_displaces_and_comes_back(self, spender, rival):
        premine = self.premine_outputs(spender)
        assert len(premine) >= 2, premine

        self.log.info("node 2 spends every premine output into one transaction")
        txid = self.spend_all_premine(spender, premine)
        assert_equal(spender.getrawmempool(), [txid])

        self.log.info("node 3 spends one of them into a transaction of its own and stakes a block carrying it")
        one = premine[0]
        raw = rival.createrawtransaction(
            [{"txid": one["txid"], "vout": one["vout"]}],
            {rival.getnewaddress(): one["amount"] - FEE})
        rival_txid = rival.sendrawtransaction(rival.signrawtransactionwithwallet(raw)["hex"])
        assert_equal(rival.getrawmempool(), [rival_txid])
        self.advance_to_next_stake_slot(nodes=[spender, rival])
        rival.generatetoaddress(1, rival.getnewaddress())
        block_hash = rival.getbestblockhash()
        assert rival_txid in rival.getblock(block_hash)["tx"]

        self.log.info("node 2 takes the block: its transaction is conflicted, the rival's is confirmed")
        genesis_hash = spender.getbestblockhash()
        self.connect_nodes(2, 3)
        self.sync_blocks([spender, rival])
        assert_equal(spender.getbestblockhash(), block_hash)
        assert_equal(spender.getrawmempool(), [])
        assert_equal(spender.gettransaction(txid)["confirmations"], -1)
        assert_equal(spender.gettransaction(rival_txid)["confirmations"], 1)

        self.stop_node(3)

        self.log.info("roll the block back: the rival spend is resurrected first, so the re-offer is refused")
        assert_equal(spender.reorganize(genesis_hash)["RollbackChain"], True)
        assert_equal(spender.getbestblockhash(), genesis_hash)
        assert_equal(spender.getrawmempool(), [rival_txid])
        assert_equal(spender.gettransaction(rival_txid)["confirmations"], 0)
        assert_equal(spender.gettransaction(txid)["confirmations"], -1)

    def descendant_follows_its_parent_back(self, spender, staker):
        premine = self.premine_outputs(spender)
        assert premine, "no premine output to spend"

        self.log.info("node 4 spends every premine output into T, then spends T's output into a child")
        txid = self.spend_all_premine(spender, premine)
        parent_out = spender.getrawtransaction(txid, True)["vout"][0]
        raw = spender.createrawtransaction(
            [{"txid": txid, "vout": parent_out["n"]}],
            {spender.getnewaddress(): Decimal(str(parent_out["value"])) - FEE})
        signed = spender.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        child = spender.sendrawtransaction(signed["hex"])
        assert_equal(sorted(spender.getrawmempool()), sorted([txid, child]))

        self.log.info("node 5 stakes a premine output; node 4 takes the block: both are conflicted")
        self.advance_to_next_stake_slot(nodes=[spender, staker])
        staker.generatetoaddress(1, staker.getnewaddress())
        block_hash = staker.getbestblockhash()
        genesis_hash = spender.getbestblockhash()
        self.connect_nodes(4, 5)
        self.sync_blocks([spender, staker])
        assert_equal(spender.getbestblockhash(), block_hash)
        assert_equal(spender.getrawmempool(), [])
        assert_equal(spender.gettransaction(txid)["confirmations"], -1)
        assert_equal(spender.gettransaction(child)["confirmations"], -1)

        self.stop_node(5)

        self.log.info("roll the block back: the child comes back with its parent")
        assert_equal(spender.reorganize(genesis_hash)["RollbackChain"], True)
        assert_equal(sorted(spender.getrawmempool()), sorted([txid, child]))
        assert_equal(spender.gettransaction(txid)["confirmations"], 0)
        assert_equal(spender.gettransaction(child)["confirmations"], 0)
        assert_equal(spender.getmempoolentry(txid)["unbroadcast"], True)
        assert_equal(spender.getmempoolentry(child)["unbroadcast"], True)

    def abandoned_transaction_is_left_alone(self, spender, staker):
        premine = self.premine_outputs(spender)
        assert premine, "no premine output to spend"

        self.log.info("node 6 spends every premine output; node 7 stakes; node 6 takes the block and abandons its transaction")
        txid = self.spend_all_premine(spender, premine)
        self.advance_to_next_stake_slot(nodes=[spender, staker])
        staker.generatetoaddress(1, staker.getnewaddress())
        block_hash = staker.getbestblockhash()
        genesis_hash = spender.getbestblockhash()
        self.connect_nodes(6, 7)
        self.sync_blocks([spender, staker])
        assert_equal(spender.getbestblockhash(), block_hash)
        assert_equal(spender.gettransaction(txid)["confirmations"], -1)
        spender.abandontransaction(txid)

        self.stop_node(7)

        self.log.info("roll the block back: an abandoned transaction is not re-offered")
        assert_equal(spender.reorganize(genesis_hash)["RollbackChain"], True)
        assert_equal(spender.getrawmempool(), [])
        assert_equal(spender.gettransaction(txid)["confirmations"], -1)
        # Its inputs are free again: abandonment released them.
        unspent = {(u["txid"], u["vout"]) for u in spender.listunspent(0)}
        for u in premine:
            assert (u["txid"], u["vout"]) in unspent


if __name__ == "__main__":
    ReorgConflictedTest().main()
