#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""PSGT pool RPC lifecycle (#2910 Phase II): the full collaborative signing
workflow driven entirely through RPC, plus the -psgtnotify hook.

Two connected regtest nodes share a 2-of-3 multisig: node0 holds one key
(the initiator), node1 holds two (a co-signer able to complete alone).

  - node0 builds and partially signs a spend via the Phase I RPCs, then
    submitpsgt pools and relays it;
  - node1's pool learns it over P2P; listpsgtpool reports ismine and
    awaiting_my_signature from the co-signer's perspective;
  - signpsgtinpool on node1 completes m-of-n: the finalized transaction is
    broadcast, and BOTH pools drain (node1 via completion, node0 via the
    mempool-conflict eviction when the transaction arrives) -- the network-
    wide completion signal;
  - the initiator supersedes a pending PSGT (different destination) with
    submitpsgt: replaced=true, same image, one pool slot;
  - removepsgtfrompool is local-only: node0 keeps its entry;
  - -psgtnotify on node1 records added/updated/completed events to a file.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

import os
import time
from decimal import Decimal


class RpcPsgtPoolTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        # extra_args are finalized in setup_network (the notify file path
        # needs the test tmpdir, unknown at set_test_params time).
        self.extra_args = [["-staking=0", "-blockv15height=0"], ["-staking=0", "-blockv15height=0"]]

    def setup_network(self):
        self.notify_file = os.path.join(self.options.tmpdir, "psgt_events.txt")
        self.extra_args[1].append("-psgtnotify=echo %s2 %s1 >> '" + self.notify_file + "'")
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_nodes(1, 0)
        self.sync_blocks()

    def fund_multisig(self, ms_address):
        """Fund the multisig from a raw spend of a consensus-mature UTXO and
        confirm it on node1.

        The confirmation block is mined on node1: its coinstake stakes node1's
        own coins (node1 holds no key for node0's funding UTXO), so it cannot
        pick the funding UTXO for a coinstake the way a node0-mined block could.

        Selection gate: the test funds via a *raw* spend of a specific
        listunspent output. Unlike a normal wallet send -- whose coin selection
        is already consistent with staking -- that manual selection can briefly
        race node0's own coinstake consuming that same premine output on the slow
        depends/musl CI (node0's wallet coin-availability lags the block that
        just staked it). So we only fund from a candidate whose spend BOTH nodes'
        consensus view accepts (testmempoolaccept), which makes the manual
        selection as consistent as normal coin selection already is and removes
        the manufactured double-spend. This is not a real-world condition -- a
        wallet never raw-spends an output its own miner is staking.
        Returns (txid, vout, amount)."""
        node0, node1 = self.nodes[0], self.nodes[1]
        self.sync_blocks()

        amount = None
        signed = None
        # 1 confirmation: the grow_utxo_universe split outputs are ordinary
        # outputs, spendable at 1 confirmation. The testmempoolaccept gate below
        # still guards spendability (and both-nodes agreement) for whatever
        # listunspent(1) returns.
        for cand in node0.listunspent(1):
            amount = cand["amount"] - 1  # ~1 GRC fee (amounts are Decimal from authproxy)
            if amount <= 0:
                continue  # too small to fund after fee (createrawtransaction rejects <= 0)
            raw = node0.createrawtransaction(
                [{"txid": cand["txid"], "vout": cand["vout"]}], {ms_address: amount})
            candidate = node0.signrawtransactionwithwallet(raw)
            if not candidate.get("complete"):
                continue
            hexes = [candidate["hex"]]
            # Only commit to an output every node agrees is genuinely unspent.
            if (node0.testmempoolaccept(hexes)[0]["allowed"]
                    and node1.testmempoolaccept(hexes)[0]["allowed"]):
                signed = candidate
                break
        assert signed is not None, "no consensus-unspent mature UTXO to fund the multisig"
        txid = node0.sendrawtransaction(signed["hex"])

        # Wait for the funding tx to reach node1's mempool, then for the next
        # 16-second STAKE_TIMESTAMP_MASK boundary (the miner excludes
        # transactions with nTime > block.nTime) before node1 mines it.
        self.wait_until(lambda: txid in node1.getrawmempool())
        time.sleep(16 - (int(time.time()) % 16) + 1)
        node1.generatetoaddress(1, node1.getnewaddress())
        self.sync_blocks()

        funding = node0.getrawtransaction(txid, True)
        assert funding.get("confirmations", 0) >= 1, "funding transaction did not confirm"
        vout = next(o["n"] for o in funding["vout"]
                    if ms_address in o["scriptPubKey"].get("addresses", []))
        return txid, vout, amount

    def initiator_psgt(self, txid, vout, amount, fee):
        """node0 builds and partially signs a spend of the multisig UTXO."""
        node0 = self.nodes[0]
        dest = node0.getnewaddress()
        psgt = node0.createpsgt([{"txid": txid, "vout": vout}],
                                {dest: amount - fee})
        psgt = node0.utxoupdatepsgt(psgt)
        processed = node0.walletprocesspsgt(psgt)
        assert not processed["complete"], "expected a PARTIALLY signed PSGT"
        return processed["psgt"]

    def run_test(self):
        node0, node1 = self.nodes[0], self.nodes[1]

        # Consensus-mature coins: coinstakes are spendable >10 blocks deep.
        node0.generatetoaddress(12, node0.getnewaddress())
        self.sync_blocks()

        # Enlarge node0's mature-UTXO pool (see grow_utxo_universe) so
        # fund_multisig's both-nodes gate always has agreed-unspent candidates --
        # the premine is only a few large outputs and node0's staking churns them,
        # briefly starving the gate ("no consensus-unspent mature UTXO") on slow
        # CI. confirm_on defaults to node0 because node1 mines the funding
        # confirmations; the gate stays as a safety net.
        self.grow_utxo_universe(node0, 48, agree_with=node1)

        # 2-of-3: node0 one key (initiator), node1 two (can complete alone).
        pub_initiator = node0.validateaddress(node0.getnewaddress())["pubkey"]
        pub_cosigner1 = node1.validateaddress(node1.getnewaddress())["pubkey"]
        pub_cosigner2 = node1.validateaddress(node1.getnewaddress())["pubkey"]
        ms_address = node0.addmultisigaddress(
            2, [pub_initiator, pub_cosigner1, pub_cosigner2])

        info = node0.getpsgtpoolinfo()
        assert_equal(info["active"], True)  # regtest activates v15 at height 0
        assert_equal(info["size"], 0)

        # --- initiate: submitpsgt pools and relays ---
        txid, vout, amount = self.fund_multisig(ms_address)
        submitted = node0.submitpsgt(self.initiator_psgt(txid, vout, amount, Decimal("0.01")))
        assert_equal(submitted["sigs_valid"], 1)
        assert_equal(submitted["sigs_required"], 2)
        assert_equal(submitted["sigs_total"], 3)
        assert_equal(submitted["ismine"], True)
        assert_equal(submitted["awaiting_my_signature"], False)  # initiator signed
        assert_equal(submitted["replaced"], False)
        assert_equal(submitted["image_address"], ms_address)
        image = submitted["image"]
        self.log.info("submitpsgt pooled the initiator's PSGT (image %s)", image)

        # --- the co-signer's node learns it over P2P ---
        self.wait_until(lambda: len(node1.listpsgtpool()) == 1)
        pooled = node1.listpsgtpool(True)[0]  # ismineonly: node1 holds keys
        assert_equal(pooled["image"], image)
        assert_equal(pooled["ismine"], True)
        assert_equal(pooled["awaiting_my_signature"], True)
        self.log.info("co-signer node pooled the PSGT and awaits a signature")

        # --- co-sign to completion: finalize, broadcast, pools drain ---
        result = node1.signpsgtinpool(image)
        assert_equal(result["complete"], True)
        final_txid = result["txid"]

        self.wait_until(lambda: final_txid in node0.getrawmempool()
                        and final_txid in node1.getrawmempool())
        self.wait_until(lambda: not node0.listpsgtpool() and not node1.listpsgtpool())
        self.log.info("signpsgtinpool completed -> %s in both mempools, pools drained",
                      final_txid)

        # Confirm the spend so the next funding round starts clean.
        node0.generatetoaddress(1, node0.getnewaddress())
        self.sync_blocks()

        # --- initiator supersede: new destination replaces the pooled PSGT ---
        txid2, vout2, amount2 = self.fund_multisig(ms_address)
        first = node0.submitpsgt(self.initiator_psgt(txid2, vout2, amount2, Decimal("0.01")))
        assert_equal(first["replaced"], False)

        revised = node0.submitpsgt(self.initiator_psgt(txid2, vout2, amount2, Decimal("0.02")))
        assert_equal(revised["replaced"], True)
        assert_equal(revised["image"], image)  # same arrangement, same slot
        assert revised["txid"] != first["txid"]
        assert_equal(len(node0.listpsgtpool()), 1)
        self.log.info("initiator superseded the pending PSGT in place")

        # --- removepsgtfrompool is local ---
        self.wait_until(
            lambda: any(e["revision"] == revised["revision"] for e in node1.listpsgtpool()))
        assert_equal(node1.removepsgtfrompool(image), True)
        assert_equal(node1.listpsgtpool(), [])
        assert_equal(len(node0.listpsgtpool()), 1)
        self.log.info("removepsgtfrompool removed locally; the initiator's node kept it")

        # --- -psgtnotify recorded the pool lifecycle on node1 ---
        # The hook runs runCommand in a detached thread, so the appended lines
        # arrive asynchronously: poll the file until every expected event lands
        # rather than reading once.
        def notify_events():
            if not os.path.exists(self.notify_file):
                return []
            with open(self.notify_file, encoding="utf8") as f:
                return [line.split()[0] for line in f if line.strip()]

        self.wait_until(lambda: {"added", "completed", "removed"}.issubset(notify_events()))
        self.log.info("-psgtnotify hook recorded: %s", notify_events())


if __name__ == "__main__":
    RpcPsgtPoolTest().main()
