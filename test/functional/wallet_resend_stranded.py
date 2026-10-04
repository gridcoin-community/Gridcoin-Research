#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A wallet transaction a reorg strands is offered back once it can be mined (#3382).

Disconnecting a block marks each of the wallet's transactions in it inactive,
and the resurrect loop then offers each back to the mempool. One that the
mempool refuses stays inactive. Nothing used to move it on from there: the wallet's
resend refuses to relay an inactive transaction, and startup recovery only
consults the transaction index. A confirmed payment silently became a
-1-confirmation entry for good, with its coins handed back as spendable.

The refusal used here is an nLockTime that is no longer reached, which is how an
HTLC refund is built. It is a time lock on a raw spend of an own coinstake
output, made non-final again by rewinding the mock clock rather than by the
shorter chain's height. That keeps every block in the case away from the
spend's input while the wallet holds it unspent: regtest has no minimum stake
age, so a block mined then could stake the very output the spend needs, and the
case would fail on that race instead of on the code under test.

  * the spend is final once the clock is past its lock time; it is sent and
    mined straight away;
  * the clock is rewound below the lock time and the block rolled back: the
    resurrection is refused (policy reports a non-final transaction as
    tx-nonstandard) and the spend is stranded at -1;
  * a forced resend while it is still non-final must not pool it;
  * once the clock is past the lock time again, a forced resend must pool it,
    at zero confirmations, and the next block must mine it.

On the unfixed daemon the last resend leaves the mempool empty and the
transaction at -1.
"""

from decimal import Decimal
from io import BytesIO

from test_framework.messages import CTransaction
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

FEE = Decimal("1.0")
# Any nSequence below the maximum makes nLockTime count.
NON_FINAL_SEQUENCE = 0xfffffffe


class WalletResendStrandedTest(GridcoinTestFramework):
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
        """The coinstake output with the most confirmations (as in
        feature_reorg_resurrect.py): the premine coinbase outputs carry one more
        confirmation than the chain height, so the filter keeps only coinstakes."""
        height = node.getblockcount()
        coinstakes = [u for u in node.listunspent(0) if u["confirmations"] <= height]
        assert coinstakes, "no coinstake output to spend"
        return max(coinstakes, key=lambda u: u["confirmations"])

    def run_test(self):
        node = self.nodes[0]

        node.generatetoaddress(10, node.getnewaddress())

        now = node.mock_now()
        lock_time = now - 1

        self.log.info("send a spend time-locked to %d, final while the clock is past it", lock_time)
        utxo = self.oldest_coinstake_utxo(node)
        raw = node.createrawtransaction(
            [{"txid": utxo["txid"], "vout": utxo["vout"]}],
            {node.getnewaddress(): utxo["amount"] - FEE})
        tx = CTransaction()
        tx.deserialize(BytesIO(bytes.fromhex(raw)))
        tx.nLockTime = lock_time
        tx.vin[0].nSequence = NON_FINAL_SEQUENCE
        signed = node.signrawtransactionwithwallet(tx.serialize().hex())
        assert_equal(signed["complete"], True)
        txid = node.sendrawtransaction(signed["hex"])
        assert_equal(node.getrawmempool(), [txid])

        self.log.info("mine it")
        rollback_hash = node.getbestblockhash()
        self.advance_to_next_stake_slot()
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getrawmempool(), [])
        assert_equal(node.gettransaction(txid)["confirmations"], 1)

        self.log.info("rewind the clock below the lock time and roll the block back: "
                      "the resurrection is refused as non-final")
        node.setmocktime(lock_time - 1)
        assert_equal(node.reorganize(rollback_hash)["RollbackChain"], True)
        assert_equal(node.getbestblockhash(), rollback_hash)
        assert_equal(node.getrawmempool(), [])
        assert_equal(node.gettransaction(txid)["confirmations"], -1)

        self.log.info("a resend while it is still non-final leaves it out of the pool")
        node.resendtx()
        assert_equal(node.getrawmempool(), [])
        assert_equal(node.gettransaction(txid)["confirmations"], -1)

        self.log.info("once the clock is past the lock time again, the resend pools it")
        # Forward past the tip, so the next block can be stamped after it.
        self.advance_to_next_stake_slot()
        node.resendtx()
        assert_equal(node.getrawmempool(), [txid])
        assert_equal(node.gettransaction(txid)["confirmations"], 0)

        self.log.info("and the next block mines it")
        self.advance_to_next_stake_slot()
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getrawmempool(), [])
        assert_equal(node.gettransaction(txid)["confirmations"], 1)
        assert_equal(node.gettransaction(txid)["blockhash"], node.getbestblockhash())


if __name__ == "__main__":
    WalletResendStrandedTest().main()
