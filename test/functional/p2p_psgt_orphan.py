#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""PSGT orphan holding pool: the unconfirmed-funding relay race (#2910 Phase II).

A node that receives a relayed `psgt` before it has seen the funding transaction
used to reject it (UTXO_MISSING) and never reconsider, so it permanently missed
a pool entry its peers held. It now HOLDS such a PSGT as an orphan and promotes
it once the funding arrives.

One node under test (node0, v15 at height 0), started with -persistmempool=0 so
a restart drops its mempool. (A second node runs only as a pubkey source and is
never connected.) node0 builds a genuinely partially signed 2-of-3 multisig PSGT
(one own key, two keys from that second node) against an UNCONFIRMED funding
transaction, then restarts -- which
clears the funding from its mempool so it no longer knows the funding output.
A scripted peer injects the PSGT and we assert:

- it is held as an orphan (getpsgtpoolinfo orphan_count == 1, size == 0) and NOT
  relayed (a watcher peer sees no inv);
- resubmitting the funding transaction promotes it: re-validated, pooled and
  relayed (size == 1, orphan_count == 0, the watcher receives inv(MSG_PSGT)).
"""

import time

from base64 import b64decode

from test_framework.messages import (
    MSG_PSGT,
    hash256,
    msg_psgt,
    uint256_from_str,
)
from test_framework.p2p import P2PInterface
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal


class InvCollector(P2PInterface):
    """Records every MSG_PSGT inventory hash it is announced."""

    def __init__(self):
        super().__init__()
        self.psgt_invs = []

    def on_inv(self, message):
        for inv in message.inv:
            if inv.type == MSG_PSGT:
                self.psgt_invs.append(inv.hash)


class P2PPsgtOrphanTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        # node0 is the node under test; -persistmempool=0 lets a restart drop the
        # unconfirmed funding. node1 is only a pubkey source (never connected).
        self.extra_args = [["-staking=0", "-blockv15height=0", "-persistmempool=0"],
                           ["-staking=0", "-blockv15height=0"]]

    def setup_network(self):
        # Deliberately do NOT connect the nodes: node1 only supplies two multisig
        # pubkeys node0 does not own (so walletprocesspsgt yields a PARTIAL sign).
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def grow_utxo_universe(self, node, count):
        """Fan one mature UTXO into `count` ordinary outputs so the funding
        selection never hits an empty listunspent.

        node0's own staking can consume the handful of premine outputs, leaving
        no mature UTXO for build_unconfirmed_funding_and_psgt -- a bare
        listunspent(11)[0] then raises IndexError on slow CI. A large pool of
        ordinary outputs (spendable at 1 confirmation, so a single confirming
        block suffices) keeps one always available. The split is a *raw*
        transaction because dev builds cripple wallet-level CommitTransaction
        (fDevbuildCripple)."""
        src = None
        for cand in node.listunspent(1):
            amount = round(float(cand["amount"]) - 1.0, 8)
            if amount <= 0:
                continue
            probe = node.signrawtransactionwithwallet(
                node.createrawtransaction(
                    [{"txid": cand["txid"], "vout": cand["vout"]}],
                    {node.getnewaddress(): amount}))
            if probe.get("complete") and node.testmempoolaccept([probe["hex"]])[0]["allowed"]:
                src = cand
                break
        assert src is not None, "no spendable UTXO to seed the universe"

        assert count > 0, "universe count must be positive"
        per_out = round((float(src["amount"]) - 1.0) / count, 8)  # ~1 GRC total fee
        assert per_out > 0, "source UTXO too small to fan out into `count` outputs"
        outputs = {node.getnewaddress(): per_out for _ in range(count)}
        signed = node.signrawtransactionwithwallet(
            node.createrawtransaction(
                [{"txid": src["txid"], "vout": src["vout"]}], outputs))
        assert signed.get("complete"), "failed to sign the universe split transaction"
        node.sendrawtransaction(signed["hex"])
        # Wait for the next 16-second STAKE_TIMESTAMP_MASK boundary before mining:
        # the miner excludes transactions with nTime > block.nTime (masked down to
        # the boundary), so without this the split tx can be left out of the block
        # and the universe is not actually enlarged.
        time.sleep(16 - (int(time.time()) % 16) + 1)
        node.generatetoaddress(1, node.getnewaddress())  # ordinary outputs: 1 conf

    def build_unconfirmed_funding_and_psgt(self):
        """On node0, create a 2-of-3 multisig (1 own key, 2 node1 keys), fund it
        with an UNCONFIRMED raw spend of a mature UTXO, and build a partially
        signed PSGT. Returns (wire_bytes, funding_hex)."""
        node0, node1 = self.nodes[0], self.nodes[1]

        pub_mine = node0.validateaddress(node0.getnewaddress())["pubkey"]
        pub_other1 = node1.validateaddress(node1.getnewaddress())["pubkey"]
        pub_other2 = node1.validateaddress(node1.getnewaddress())["pubkey"]
        ms_address = node0.addmultisigaddress(2, [pub_mine, pub_other1, pub_other2])

        # Pick a spendable output and build the funding tx in one pass:
        # grow_utxo_universe guarantees a supply of ordinary 1-confirmation
        # outputs, so 1 confirmation suffices; a bare listunspent(11)[0] could
        # hit an empty list once staking has consumed the mature premine outputs.
        utxo = None
        for cand in node0.listunspent(1):
            ms_amount = round(float(cand["amount"]) - 1.0, 8)  # ~1 GRC fee
            if ms_amount <= 0.01:  # need enough for the funding + the PSGT's 0.01 fee
                continue
            raw = node0.createrawtransaction(
                [{"txid": cand["txid"], "vout": cand["vout"]}], {ms_address: ms_amount})
            signed = node0.signrawtransactionwithwallet(raw)
            if signed.get("complete") and node0.testmempoolaccept([signed["hex"]])[0]["allowed"]:
                utxo = cand
                break
        assert utxo is not None, "no spendable mature UTXO to fund the multisig"
        funding_hex = signed["hex"]
        txid = node0.sendrawtransaction(funding_hex)  # unconfirmed, mempool only

        funding = node0.getrawtransaction(txid, True)
        vout = next(o["n"] for o in funding["vout"]
                    if ms_address in o["scriptPubKey"].get("addresses", []))

        dest = node0.getnewaddress()
        psgt = node0.createpsgt([{"txid": txid, "vout": vout}],
                                {dest: round(ms_amount - 0.01, 8)})
        psgt = node0.utxoupdatepsgt(psgt)
        processed = node0.walletprocesspsgt(psgt)
        assert not processed["complete"], "expected a PARTIALLY signed PSGT"
        return b64decode(processed["psgt"]), funding_hex

    def run_test(self):
        node0 = self.nodes[0]

        # Consensus-mature coins (>10 confirmations) and a current tip.
        node0.generatetoaddress(12, node0.getnewaddress())
        # Enlarge the mature-UTXO pool so build_unconfirmed_funding_and_psgt
        # never finds an empty listunspent -- node0's staking can otherwise
        # deplete the few premine outputs and raise IndexError on slow CI.
        self.grow_utxo_universe(node0, count=48)

        wire, funding_hex = self.build_unconfirmed_funding_and_psgt()
        revision = uint256_from_str(hash256(wire))

        # Restart: -persistmempool=0 drops the unconfirmed funding, so node0 no
        # longer knows the funding output -- exactly the relay-race condition.
        self.restart_node(0, self.extra_args[0])
        assert_equal(node0.getrawmempool(), [])

        watcher = node0.add_p2p_connection(InvCollector())
        time.sleep(6)  # inbound rate limit: 1 per 5s per IP (all 127.0.0.1)
        sender = node0.add_p2p_connection(P2PInterface())

        # --- inject the PSGT while the funding is unknown: held, not pooled ---
        sender.send_message(msg_psgt(wire))
        sender.sync_with_ping()
        time.sleep(1)  # allow any (wrong) relay to arrive

        info = node0.getpsgtpoolinfo()
        assert_equal(info["orphan_count"], 1)
        assert_equal(info["size"], 0)
        assert_equal(watcher.psgt_invs, [])
        self.log.info("PSGT held as orphan (funding unknown); not pooled, not relayed")

        # --- the funding transaction arrives -> promote, pool, relay ---
        node0.sendrawtransaction(funding_hex)
        watcher.wait_until(lambda: revision in watcher.psgt_invs)
        info = node0.getpsgtpoolinfo()
        assert_equal(info["size"], 1)
        assert_equal(info["orphan_count"], 0)
        self.log.info("funding arrived -> orphan re-validated, pooled, and relayed")


if __name__ == "__main__":
    P2PPsgtOrphanTest().main()
