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
        """Fund the multisig from a raw spend of a consensus-mature UTXO
        (>10 confirmations) and confirm it. The wallet does not track raw
        spends, so the staker can race us for the same UTXO when mining the
        confirmation block and double-spend the funding away: verify the
        funding confirmed and retry with a fresh UTXO if not.
        Returns (txid, vout, amount)."""
        node0 = self.nodes[0]
        tried = set()
        for _ in range(5):
            candidates = [u for u in node0.listunspent(11)
                          if (u["txid"], u["vout"]) not in tried]
            assert candidates, "no mature UTXO left to fund the multisig"
            utxo = candidates[0]
            tried.add((utxo["txid"], utxo["vout"]))

            amount = round(float(utxo["amount"]) - 1.0, 8)  # ~1 GRC fee
            raw = node0.createrawtransaction(
                [{"txid": utxo["txid"], "vout": utxo["vout"]}], {ms_address: amount})
            signed = node0.signrawtransactionwithwallet(raw)
            assert signed.get("complete"), signed
            txid = node0.sendrawtransaction(signed["hex"])

            # PoS block timestamps are masked down to 16-second boundaries
            # (STAKE_TIMESTAMP_MASK) and the miner excludes transactions with
            # nTime > block.nTime: wait for the next boundary before mining
            # the confirmation block.
            time.sleep(16 - (int(time.time()) % 16) + 1)
            node0.generatetoaddress(1, node0.getnewaddress())

            try:
                funding = node0.getrawtransaction(txid, True)
                if funding.get("confirmations", 0) >= 1:
                    self.sync_blocks()
                    vout = next(o["n"] for o in funding["vout"]
                                if ms_address in o["scriptPubKey"].get("addresses", []))
                    return txid, vout, amount
            except Exception:
                pass  # double-spent by the staker's coinstake: retry

        raise AssertionError("could not confirm the multisig funding transaction")

    def initiator_psgt(self, txid, vout, amount, fee):
        """node0 builds and partially signs a spend of the multisig UTXO."""
        node0 = self.nodes[0]
        dest = node0.getnewaddress()
        psgt = node0.createpsgt([{"txid": txid, "vout": vout}],
                                {dest: round(amount - fee, 8)})
        psgt = node0.utxoupdatepsgt(psgt)
        processed = node0.walletprocesspsgt(psgt)
        assert not processed["complete"], "expected a PARTIALLY signed PSGT"
        return processed["psgt"]

    def run_test(self):
        node0, node1 = self.nodes[0], self.nodes[1]

        # Consensus-mature coins: coinstakes are spendable >10 blocks deep.
        node0.generatetoaddress(12, node0.getnewaddress())
        self.sync_blocks()

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
        submitted = node0.submitpsgt(self.initiator_psgt(txid, vout, amount, 0.01))
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
        first = node0.submitpsgt(self.initiator_psgt(txid2, vout2, amount2, 0.01))
        assert_equal(first["replaced"], False)

        revised = node0.submitpsgt(self.initiator_psgt(txid2, vout2, amount2, 0.02))
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
