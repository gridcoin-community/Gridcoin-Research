#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Phase 3 P2P relay test: forward a node-produced tx and block over P2P.

Two ISOLATED regtest nodes (no daemon-to-daemon connection). node0 produces a
real, valid transaction and block; a framework P2PDataStore peer connected to
node1 relays them, and we assert node1 accepts each. This exercises the tx and
block wire serialization end to end against a live daemon.

The artifacts are always node-produced (never forged in Python): constructing a
valid PoS coinstake from Python is infeasible, so node0 does the signing/mining
and we relay the bytes. As a serialization check we assert our deserialize ->
serialize is byte-exact against the node's hex (so our computed txid / block
hash equals the node's).
"""

from io import BytesIO

from test_framework.test_framework import GridcoinTestFramework
from test_framework.messages import CInv, CTransaction, MSG_BLOCK, msg_getdata
from test_framework.p2p import P2PDataStore, P2PInterface
from test_framework.util import assert_equal


class P2PBlockTxRelayTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0"], ["-staking=0"]]

    def setup_network(self):
        # Bring up two regtest nodes but do NOT connect them to each other, so
        # the only path for tx/block propagation to node1 is our P2P peer.
        # Also skip the base regtest coinbase import (createwallet) path.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node0, node1 = self.nodes[0], self.nodes[1]
        peer = node1.add_p2p_connection(P2PDataStore())

        # --- tx relay: node0 builds+signs a tx spending a premine UTXO; the
        # peer relays it to node1, which should accept it into its mempool. ---
        # getbalance() reports 0 (premine coinbase is "immature" for balance
        # accounting), so build the tx directly from a listunspent premine
        # UTXO rather than via sendtoaddress. Amounts are floats so they
        # serialize as JSON numbers -- Gridcoin's AmountFromValue rejects
        # string amounts (which is what a Decimal becomes via EncodeDecimal).
        u = node0.listunspent(0)[0]
        sink = node0.getnewaddress()
        raw = node0.createrawtransaction(
            [{"txid": u["txid"], "vout": u["vout"]}], {sink: 99999.0})  # ~1 GRC fee
        signed = node0.signrawtransactionwithwallet(raw)
        assert signed.get("complete"), signed
        rawhex = signed["hex"]
        tx = CTransaction()
        tx.deserialize(BytesIO(bytes.fromhex(rawhex)))
        tx.rehash()
        assert_equal(tx.serialize().hex(), rawhex)   # byte-exact tx (de)serialization

        # send_txs_and_test(success=True) already asserts the tx lands in node1's
        # mempool, so no separate getrawmempool() check is needed here.
        peer.send_txs_and_test([tx], node1, success=True)
        self.log.info("tx relay OK: %s accepted into node1 mempool", tx.hash)

        # --- block relay: node0 mines a block; fetch it from node0 over P2P
        # (Gridcoin's getblock has no raw-hex mode -- it always returns a JSON
        # object), then relay it to node1. This exercises block serialization
        # in BOTH directions: receiving node0's `block` message and re-sending
        # it to node1. ---
        blockhash = node0.generatetoaddress(1, sink)[0]
        hashint = int(blockhash, 16)   # display hex == our internal sha256 int
        src = node0.add_p2p_connection(P2PInterface())
        src.send_message(msg_getdata([CInv(MSG_BLOCK, hashint)]))
        src.wait_for_block(hashint)
        block = src.last_message["block"].block
        block.rehash()
        assert_equal(block.hash, blockhash)          # our parse matches node0's block hash

        # force_send pushes the full `block` message directly rather than
        # relying on the headers/getdata announcement dance, keeping the test
        # independent of the headers-message wire details.
        peer.send_blocks_and_test([block], node1, success=True, force_send=True)
        assert_equal(node1.getbestblockhash(), blockhash)
        self.log.info("block relay OK: fetched node0's block over P2P; node1 tip -> %s", blockhash)

        node0.disconnect_p2ps()
        node1.disconnect_p2ps()


if __name__ == "__main__":
    P2PBlockTxRelayTest().main()
