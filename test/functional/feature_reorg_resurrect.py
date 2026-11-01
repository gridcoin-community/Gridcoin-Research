#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A wallet transaction survives its block being disconnected.

DisconnectBlocksBatch queues every ordinary transaction of a disconnected
block and re-submits it to the mempool, so a transaction that was mined and
then reorganized away is still pending afterwards unless it is independently
invalid at the new tip: the next block picks it up again, and the wallet keeps
reporting it at zero confirmations.

The regtest `reorganize` RPC rolls the chain back to a given hash, so one node
walks the whole path: spend a mature coinstake output to an own address, mine
the transaction, roll the chain back one block, and mine again.

Each step asserts the state only a working resurrection produces:

  * after the roll-back the transaction is in the mempool, gettransaction
    reports it at zero confirmations (a transaction that is in neither a block
    nor the mempool reports -1), and the output it spends is not offered as
    unspent;
  * the next block mines it again and the mempool is empty.

The re-acceptance used to be refused for every transaction: it ran before the
disconnect batch was committed to the transaction index, so the mempool still
saw the transaction as confirmed and its inputs as spent. Nothing was logged
and the mempool stayed empty. And once it did land, FixSpentCoins, which runs
right after the disconnect and consulted only the tx index, handed the
transaction's input back as spendable.
"""

from decimal import Decimal

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

FEE = Decimal("1.0")


class ReorgResurrectTest(GridcoinTestFramework):
    def set_test_params(self):
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        # Single isolated regtest node; bypass the base regtest createwallet
        # path (Gridcoin has one default BDB wallet, no multiwallet).
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def oldest_coinstake_utxo(self, node):
        """The coinstake output with the most confirmations.

        The height-0 premine coinbase outputs carry one more confirmation than
        the chain height, so the filter keeps every post-genesis output. All of
        those are coinstakes here, and the oldest one is the furthest from any
        maturity rule.
        """
        height = node.getblockcount()
        coinstakes = [u for u in node.listunspent(0) if u["confirmations"] <= height]
        assert coinstakes, "no coinstake output to spend"
        return max(coinstakes, key=lambda u: u["confirmations"])

    def unspent_outpoints(self, node):
        return {(u["txid"], u["vout"]) for u in node.listunspent(0)}

    def run_test(self):
        node = self.nodes[0]

        node.generatetoaddress(10, node.getnewaddress())

        self.log.info("spend a mature coinstake output to an own address")
        utxo = self.oldest_coinstake_utxo(node)
        dest = node.getnewaddress()
        raw = node.createrawtransaction(
            [{"txid": utxo["txid"], "vout": utxo["vout"]}], {dest: utxo["amount"] - FEE})
        signed = node.signrawtransactionwithwallet(raw)
        assert_equal(signed["complete"], True)
        txid = node.sendrawtransaction(signed["hex"])
        assert_equal(node.getrawmempool(), [txid])
        assert_equal(node.gettransaction(txid)["confirmations"], 0)

        self.log.info("mine it")
        self.advance_to_next_stake_slot()
        base_hash = node.getbestblockhash()
        base_height = node.getblockcount()
        node.generatetoaddress(1, node.getnewaddress())
        mined_hash = node.getbestblockhash()
        assert_equal(node.getrawmempool(), [])
        assert_equal(node.gettransaction(txid)["confirmations"], 1)
        assert_equal(node.gettransaction(txid)["blockhash"], mined_hash)

        self.log.info("roll the chain back one block: the transaction is pending again")
        assert_equal(node.reorganize(base_hash)["RollbackChain"], True)
        assert_equal(node.getbestblockhash(), base_hash)
        assert_equal(node.getrawmempool(), [txid])
        assert_equal(node.gettransaction(txid)["confirmations"], 0)
        # The input is spent by a pending transaction, so it must not be
        # offered for spending or staking again.
        assert (utxo["txid"], utxo["vout"]) not in self.unspent_outpoints(node)

        self.log.info("the next block mines it again")
        self.advance_to_next_stake_slot()
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getblockcount(), base_height + 1)
        assert node.getbestblockhash() != mined_hash
        assert_equal(node.getrawmempool(), [])
        assert_equal(node.gettransaction(txid)["confirmations"], 1)
        assert_equal(node.gettransaction(txid)["blockhash"], node.getbestblockhash())


if __name__ == "__main__":
    ReorgResurrectTest().main()
