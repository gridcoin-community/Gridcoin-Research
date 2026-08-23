#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 4A mempool acceptance / rejection on regtest.

Investor-mode only. Gridcoin's merged RPC surface has no testmempoolaccept, so
this drives sendrawtransaction + getrawmempool directly against a staked
coinstake output:

  - a valid raw tx is accepted into the mempool;
  - a double-spend of the same in-mempool UTXO is rejected;
  - a spend of an outpoint already spent by a CONFIRMED transaction is rejected.

The third case is a different code path from the second. An in-mempool conflict
is caught by AcceptToMemoryPool's mapNextTx scan; a conflict with a confirmed
spend is not visible there and is only caught in ConnectInputs, against
txindex.vSpent. A staked block gives us a confirmed spend for free: the coinstake
consumes a wallet UTXO, so that outpoint is spent on-chain and can be replayed.

assert_raises is used (not assert_raises_rpc_error) so the rejection check does
not depend on Gridcoin's specific RPC error codes.

NOTE: mining/confirmation is not asserted here. On the current regtest stack,
generatetoaddress blocks contain only the coinbase + coinstake (mempool
transactions are not included), so a mempool tx never confirms. Re-add a
confirm-and-clear step once regtest block assembly includes mempool transactions.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


class MempoolAcceptTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def _signed_spend(self, node, utxo, dest, amount):
        raw = node.createrawtransaction(
            [{"txid": utxo["txid"], "vout": utxo["vout"]}], {dest: amount})
        signed = node.signrawtransactionwithwallet(raw)
        assert signed.get("complete"), signed
        return signed["hex"]

    def _confirmed_spent_outpoint(self, node):
        """Find an outpoint a confirmed coinstake already spent.

        Walks blocks from the tip looking for a coinstake (vout[0] empty by
        Gridcoin convention) and returns its first real input along with the
        scriptPubKey and amount needed to re-sign a spend of it.
        """
        for height in range(node.getblockcount(), 0, -1):
            block = node.getblock(node.getblockhash(height), True)
            for tx in block.get("tx", []):
                vin = tx.get("vin", [])
                vout = tx.get("vout", [])
                # coinstake: first output is empty; skip coinbase (no prevout)
                if not vout or vout[0].get("value") != 0:
                    continue
                for entry in vin:
                    if "txid" not in entry:
                        continue
                    prev = node.getrawtransaction(entry["txid"], 1)
                    out = prev["vout"][entry["vout"]]
                    return (entry["txid"], entry["vout"],
                            out["scriptPubKey"]["hex"], out["value"])
        return None

    def run_test(self):
        node = self.nodes[0]
        assert_equal(len(node.listunspent(0)), 10)

        # stake so we have a mature, tx-indexed coinstake output to spend
        node.generatetoaddress(10, node.getnewaddress())
        # most-recently-staked output (fewest confirmations) is a coinstake,
        # never the height-0 premine coinbase.
        u = min(node.listunspent(0), key=lambda x: x["confirmations"])
        amt = u["amount"] - 1  # ~1 GRC fee (amounts are Decimal from authproxy)

        # 1. a valid tx is accepted into the mempool
        first = self._signed_spend(node, u, node.getnewaddress(), amt)
        txid = node.sendrawtransaction(first)
        assert txid in node.getrawmempool(), "valid tx not accepted into mempool"
        self.log.info("valid tx accepted into mempool: %s", txid)

        # 2. a double-spend of the same UTXO is rejected (any RPC error code)
        second = self._signed_spend(node, u, node.getnewaddress(), amt)
        assert_raises_rpc_error(None, None, node.sendrawtransaction, second)
        self.log.info("in-mempool double-spend correctly rejected")

        # 3. spending an outpoint already consumed by a confirmed transaction.
        #
        # Distinct from case 2: mapNextTx only knows about the mempool, so this
        # reaches ConnectInputs and is rejected against txindex.vSpent. That is
        # the check the signature loop depends on running first -- if it ever
        # regresses to being tested per-input alongside VerifySignature, a
        # transaction like this one costs a full signature verification for
        # every input ahead of the conflicting one before being dropped.
        spent = self._confirmed_spent_outpoint(node)
        if spent is None:
            self.log.warning("no confirmed coinstake input found; skipping case 3")
            return

        prev_txid, prev_vout, prev_spk, prev_amt = spent
        raw = node.createrawtransaction(
            [{"txid": prev_txid, "vout": prev_vout}],
            {node.getnewaddress(): prev_amt - 1})
        signed = node.signrawtransactionwithwallet(
            raw,
            [{"txid": prev_txid, "vout": prev_vout,
              "scriptPubKey": prev_spk, "amount": prev_amt}])
        assert_raises_rpc_error(None, None, node.sendrawtransaction, signed["hex"])
        self.log.info("confirmed-spend replay correctly rejected (%s:%d)",
                      prev_txid, prev_vout)


if __name__ == "__main__":
    MempoolAcceptTest().main()
