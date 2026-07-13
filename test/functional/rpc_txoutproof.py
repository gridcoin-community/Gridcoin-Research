#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Transaction inclusion proofs (gettxoutproof / verifytxoutproof) on regtest.

Investor-mode only. Drives the SPV-style Merkle proof surface against confirmed
coinstake outputs (tx-indexed, full txindex is on by default):

  - gettxoutproof([txid]) emits a hex-encoded Merkle proof;
  - verifytxoutproof(proof) validates it and returns the committed txid(s);
  - round-trip works both with and without an explicit blockhash;
  - negative cases: unknown txid, unconfirmed (mempool) txid, and a corrupted
    proof are all rejected.
"""

from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


class RpcTxoutProofTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-connect=0", "-listen=0"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        node = self.nodes[0]

        # Stake blocks so we have confirmed, tx-indexed coinstake outputs. Each PoS
        # block carries one coinstake tx; the height-0 premine is a single coinbase
        # tx (10 outputs, one txid) that is NOT in the regtest tx index, so it cannot
        # be used as a proof subject. We therefore drive proofs off coinstakes.
        node.generatetoaddress(15, node.getnewaddress())

        # Collect the two most-recently-staked (lowest-confirmation) distinct txids.
        # Coinstakes sort ahead of the high-confirmation premine, and consecutive
        # coinstakes live in different blocks, so these are two tx-indexed txids in
        # two different blocks.
        distinct = []
        seen = set()
        for u in sorted(node.listunspent(0), key=lambda u: u["confirmations"]):
            if u["confirmations"] < 1 or u["txid"] in seen:
                continue
            seen.add(u["txid"])
            distinct.append((u["txid"], node.gettransaction(u["txid"])["blockhash"]))
            if len(distinct) >= 2:
                break
        assert len(distinct) >= 2, "need two confirmed coinstake txids"
        (txid1, blockhash1), (txid2, blockhash2) = distinct[0], distinct[1]
        assert blockhash1 != blockhash2, "expected coinstakes in different blocks"

        # --- round-trip without blockhash ---
        proof = node.gettxoutproof([txid1])
        assert isinstance(proof, str) and proof, "empty proof"
        assert_equal(node.verifytxoutproof(proof), [txid1])
        self.log.info("gettxoutproof/verifytxoutproof round-trip OK (no blockhash)")

        # --- round-trip with an explicit blockhash ---
        proof_bh = node.gettxoutproof([txid1], blockhash1)
        assert_equal(node.verifytxoutproof(proof_bh), [txid1])
        self.log.info("round-trip OK with explicit blockhash")

        # --- same-block constraint ---
        # txid2 lives in a different block than blockhash1, so requesting both
        # against blockhash1 must fail because not all txids are in that block.
        assert_raises_rpc_error(
            -5, "Not all transactions found in specified or retrieved block",
            node.gettxoutproof, [txid1, txid2], blockhash1)
        self.log.info("same-block constraint enforced for multi-txid proofs")

        # --- negative: unknown txid ---
        unknown = "00" * 32
        assert_raises_rpc_error(
            -5, "Transaction not yet in block",
            node.gettxoutproof, [unknown])
        self.log.info("unknown txid rejected")

        # --- negative: unconfirmed (mempool) txid ---
        # Build a raw spend of a recent coinstake output and broadcast it. On this
        # stack generatetoaddress excludes mempool txs, so it stays unconfirmed and
        # has no containing block. (sendtoaddress is avoided: its coin selection can
        # pick the non-tx-indexed premine coinbase, which the wallet then rejects.)
        cs = min(node.listunspent(0), key=lambda u: u["confirmations"])
        dest = node.getnewaddress()
        raw = node.createrawtransaction(
            [{"txid": cs["txid"], "vout": cs["vout"]}], {dest: cs["amount"] - 1})
        signed = node.signrawtransactionwithwallet(raw)
        assert signed.get("complete"), signed
        mempool_txid = node.sendrawtransaction(signed["hex"])
        assert mempool_txid in node.getrawmempool(), "tx did not enter mempool"
        assert_raises_rpc_error(
            -5, "Transaction not yet in block",
            node.gettxoutproof, [mempool_txid])
        self.log.info("unconfirmed txid rejected")

        # --- negative: corrupted proof ---
        assert_raises_rpc_error(
            -22, "Proof could not be deserialized",
            node.verifytxoutproof, "00")
        self.log.info("corrupted proof rejected")

        # --- a structurally valid proof that fails root reconstruction -> empty ---
        # The serialized header is nVersion(4) + hashPrevBlock(32) + hashMerkleRoot(32)...
        # Flip one nibble inside hashMerkleRoot (hex offset 72..136) so the stored
        # root no longer matches the root recomputed from the tree. The proof still
        # parses, but verifytxoutproof returns an empty array instead of throwing.
        i = 80
        flipped = "0" if proof[i] != "0" else "1"
        garbled = proof[:i] + flipped + proof[i + 1:]
        assert_equal(node.verifytxoutproof(garbled), [])
        self.log.info("proof failing root reconstruction returns empty array")


if __name__ == "__main__":
    RpcTxoutProofTest().main()
