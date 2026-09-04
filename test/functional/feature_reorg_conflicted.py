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
"""

from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

FEE = Decimal("1.0")


class ReorgConflictedTest(GridcoinTestFramework):
    def set_test_params(self):
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.num_nodes = 2
        # Isolated: the conflict needs node 1 to stake in ignorance of node 0's
        # transaction, so the two must not share a mempool until afterwards.
        self.extra_args = [["-staking=0", "-connect=0", "-listen=1"]] * 2

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

    def run_test(self):
        spender, staker = self.nodes

        premine = self.premine_outputs(spender)
        assert premine, "no premine output to spend"
        self.log.info("both nodes hold the same %d premine outputs", len(premine))
        assert_equal(len(self.premine_outputs(staker)), len(premine))

        self.log.info("node 0 spends every premine output into one transaction")
        total = sum(u["amount"] for u in premine)
        raw = spender.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]} for u in premine],
            {spender.getnewaddress(): total - FEE})
        signed = spender.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        txid = spender.sendrawtransaction(signed["hex"])
        assert_equal(spender.getrawmempool(), [txid])
        assert_equal(spender.gettransaction(txid)["confirmations"], 0)

        self.log.info("node 1, which has not seen it, stakes a premine output")
        self.advance_to_next_stake_slot()
        staker.generatetoaddress(1, staker.getnewaddress())
        block_hash = staker.getbestblockhash()
        assert_equal(staker.getblockcount(), 1)

        self.log.info("node 0 takes that block: its transaction is conflicted")
        genesis_hash = spender.getbestblockhash()
        assert_equal(spender.getblockcount(), 0)
        self.connect_nodes(0, 1)
        self.sync_blocks()
        assert_equal(spender.getbestblockhash(), block_hash)
        assert_equal(spender.getrawmempool(), [])
        assert_equal(spender.gettransaction(txid)["confirmations"], -1)

        # Stop the staker so it cannot re-announce its branch over the rollback.
        self.stop_node(1)

        self.log.info("roll the block back: the conflict is gone, so the transaction is pending again")
        assert_equal(spender.reorganize(genesis_hash)["RollbackChain"], True)
        assert_equal(spender.getbestblockhash(), genesis_hash)
        assert_equal(spender.getrawmempool(), [txid])
        assert_equal(spender.gettransaction(txid)["confirmations"], 0)

        # And the wallet treats it as pending throughout: the outputs it spends
        # are not offered for spending or staking again. (There is no next block
        # to mine it into -- the transaction consumed every output this node
        # could stake, which is what made the conflict deterministic.)
        unspent = {(u["txid"], u["vout"]) for u in spender.listunspent(0)}
        for u in premine:
            assert (u["txid"], u["vout"]) not in unspent


if __name__ == "__main__":
    ReorgConflictedTest().main()
