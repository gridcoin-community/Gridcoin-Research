#!/usr/bin/env python3
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""PSGT pool P2P relay (#2910 Phase II): MSG_PSGT inv/getdata/psgt end to end.

Two connected regtest nodes (v15 active at height 0) plus scripted peers.
node0's wallet builds a genuinely partially signed 2-of-3 multisig PSGT via
the Phase I RPCs (one wallet-owned key, two node1-owned keys, so
walletprocesspsgt contributes exactly one of the two required signatures).
A scripted peer then injects the raw bytes into node1 with the new `psgt`
message and we assert, in order:

- node1 validates, pools and relays: a watcher peer on node1 receives
  inv(MSG_PSGT, revision-hash) where the revision hash is hash256 of the
  wire bytes (NOT the unsigned tx hash);
- getdata(MSG_PSGT) is answered byte-exactly from the pool;
- node0 learns the PSGT daemon-to-daemon, and a peer connecting to node0
  afterwards receives the pooled inventory in the connect-time push;
- a peer that handshook with the pre-PSGT protocol version receives no
  MSG_PSGT inventory anywhere in the run;
- re-sending the same revision is a quiet no-op (no second inv);
- garbage and corrupted psgt messages are rejected without relay and
  without disturbing the node. (The misbehavior score they earn cannot
  be observed from a functional test: PeerManager::Misbehaving exempts
  local addresses, and every scripted peer here is 127.0.0.1. The
  scoring itself is covered by inspection of the ProcessPSGTMessage
  reject mapping.)
"""

import time

from test_framework.messages import (
    CInv,
    MSG_PSGT,
    PSGT_PROTO_VERSION,
    hash256,
    msg_getdata,
    msg_psgt,
    uint256_from_str,
)
from test_framework.p2p import P2PInterface
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

from base64 import b64decode


class InvCollector(P2PInterface):
    """Records every MSG_PSGT inventory hash it is announced."""

    def __init__(self):
        super().__init__()
        self.psgt_invs = []

    def on_inv(self, message):
        for inv in message.inv:
            if inv.type == MSG_PSGT:
                self.psgt_invs.append(inv.hash)


class OldVersionInvCollector(InvCollector):
    """An InvCollector that handshakes with the pre-PSGT protocol version."""

    def peer_connect_send_version(self):
        super().peer_connect_send_version()
        self.on_connection_send_msg.nVersion = PSGT_PROTO_VERSION - 1


class P2PPsgtRelayTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.chain = "regtest"
        self.setup_clean_chain = True
        self.extra_args = [["-staking=0", "-blockv15height=0"], ["-staking=0", "-blockv15height=0"]]

    def confirm_funding(self, txid):
        """Confirm a just-sent funding transaction. PoS block timestamps are
        masked down to 16-second boundaries (STAKE_TIMESTAMP_MASK) and the
        miner excludes transactions with nTime > block.nTime, so wait for the
        next boundary before mining the confirmation block. Returns False if
        the transaction vanished instead (double-spent by the staker's own
        coinstake -- the wallet does not track raw spends)."""
        node0 = self.nodes[0]
        time.sleep(16 - (int(time.time()) % 16) + 1)
        node0.generatetoaddress(1, node0.getnewaddress())
        try:
            return node0.getrawtransaction(txid, True).get("confirmations", 0) >= 1
        except Exception:
            return False

    def add_spaced_p2p(self, node, peer):
        """add_p2p_connection with the daemon's inbound rate limit respected:
        at most one inbound per 5 seconds per IP (net.cpp AcceptConnection),
        and every scripted peer here is 127.0.0.1. The limit compares integer
        second deltas from GetAdjustedTime(), so sleep a full 6 s -- sleeping
        exactly 5 can land two connects in the same second boundary (delta 4)
        and get the second one dropped / misbehavior-scored."""
        time.sleep(6)
        return node.add_p2p_connection(peer)

    def setup_network(self):
        # Bring the nodes up without the base-class createwallet path (the
        # daemon rejects its named params), then connect them: daemon-to-daemon
        # PSGT propagation is part of what this test asserts.
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        self.connect_nodes(1, 0)
        self.sync_blocks()

    def make_partially_signed_psgt(self):
        """Fund a 2-of-3 multisig (1 node0 key, 2 node1 keys) and have node0
        sign its one key: a valid, incomplete PSGT the pool must accept."""
        node0, node1 = self.nodes[0], self.nodes[1]

        pub_mine = node0.validateaddress(node0.getnewaddress())["pubkey"]
        pub_other1 = node1.validateaddress(node1.getnewaddress())["pubkey"]
        pub_other2 = node1.validateaddress(node1.getnewaddress())["pubkey"]
        ms_address = node0.addmultisigaddress(2, [pub_mine, pub_other1, pub_other2])

        # Fund the multisig from a raw spend of a consensus-mature UTXO
        # (coinstakes need >10 confirmations on regtest; wallet sends are not
        # an option -- wallet balance accounting treats young coinstakes as
        # immature far longer). The wallet does not track raw spends, so the
        # staker can race us for the same UTXO when mining the confirmation
        # block and double-spend the funding away: verify and retry.
        txid = None
        ms_amount = None
        tried = set()
        for _ in range(5):
            # 1 confirmation: grow_utxo_universe's split outputs are ordinary
            # outputs, spendable immediately; confirm_funding + this retry loop
            # still absorb the staker racing us for a chosen output.
            candidates = [u for u in node0.listunspent(1)
                          if (u["txid"], u["vout"]) not in tried
                          and round(float(u["amount"]) - 1.0, 8) > 0.01]
            assert candidates, "no mature UTXO left to fund the multisig"
            utxo = candidates[0]
            tried.add((utxo["txid"], utxo["vout"]))

            ms_amount = round(float(utxo["amount"]) - 1.0, 8)  # ~1 GRC fee
            raw = node0.createrawtransaction(
                [{"txid": utxo["txid"], "vout": utxo["vout"]}], {ms_address: ms_amount})
            signed = node0.signrawtransactionwithwallet(raw)
            assert signed.get("complete"), signed
            candidate_txid = node0.sendrawtransaction(signed["hex"])

            if self.confirm_funding(candidate_txid):
                txid = candidate_txid
                break
        assert txid, "could not confirm the multisig funding transaction"
        self.sync_blocks()

        funding = node0.getrawtransaction(txid, True)
        vout = next(o["n"] for o in funding["vout"]
                    if ms_address in o["scriptPubKey"].get("addresses", []))

        # 0.01 GRC fee: above the relay floor, below the absurd-fee ceiling.
        dest = node0.getnewaddress()
        psgt = node0.createpsgt([{"txid": txid, "vout": vout}],
                                {dest: round(ms_amount - 0.01, 8)})
        psgt = node0.utxoupdatepsgt(psgt)
        processed = node0.walletprocesspsgt(psgt)
        assert not processed["complete"], "expected a PARTIALLY signed PSGT"
        return b64decode(processed["psgt"])

    def run_test(self):
        node0, node1 = self.nodes[0], self.nodes[1]

        # Consensus-mature coins (>10 confirmations on regtest) and a current
        # tip so both nodes are in sync (the pool neither accepts nor serves
        # while OutOfSyncByAge).
        node0.generatetoaddress(12, node0.getnewaddress())
        self.sync_blocks()
        # Enlarge the mature-UTXO pool so make_partially_signed_psgt's candidate
        # scan never empties (node0's staking can deplete the few premine
        # outputs). Confirmed on node1 -- node0 mines the funding confirmations,
        # so splitting there too would eat into node0's stake budget.
        self.grow_utxo_universe(node0, 48, agree_with=node1, confirm_on=node1)

        wire = self.make_partially_signed_psgt()
        revision = uint256_from_str(hash256(wire))

        sender = node1.add_p2p_connection(P2PInterface())
        watcher = self.add_spaced_p2p(node1, InvCollector())
        old_peer = self.add_spaced_p2p(node1, OldVersionInvCollector())

        # --- inject: node1 must validate, pool, and announce the revision ---
        sender.send_message(msg_psgt(wire))
        watcher.wait_until(lambda: revision in watcher.psgt_invs)
        self.log.info("psgt accepted by node1 and announced as inv(MSG_PSGT)")

        # --- getdata is served byte-exactly from the pool ---
        watcher.send_message(msg_getdata([CInv(MSG_PSGT, revision)]))
        watcher.wait_until(lambda: "psgt" in watcher.last_message
                           and watcher.last_message["psgt"].psgt_bytes == wire)
        self.log.info("getdata(MSG_PSGT) served the exact wire bytes")

        # --- daemon-to-daemon relay + connect-time push: node0 fetched the
        # PSGT from node1, and advertises it to fresh v15 peers on connect ---
        late_peer = self.add_spaced_p2p(node0, InvCollector())
        late_peer.wait_until(lambda: revision in late_peer.psgt_invs)
        self.log.info("node0 pooled the PSGT and pushed it to a connecting peer")

        # --- duplicate revision: a quiet no-op, not a re-announcement ---
        announced_before = watcher.psgt_invs.count(revision)
        sender.send_message(msg_psgt(wire))
        sender.sync_with_ping()
        time.sleep(1)  # allow any (wrong) re-announcement to arrive
        assert_equal(watcher.psgt_invs.count(revision), announced_before)
        self.log.info("duplicate psgt message did not re-announce")

        # --- version gating: the 180329 peer saw nothing throughout ---
        old_peer.sync_with_ping()
        assert_equal(old_peer.psgt_invs, [])
        self.log.info("pre-PSGT protocol peer received no MSG_PSGT inventory")

        # --- invalid psgt messages: rejected without relay, node unfazed ---
        bad_peer = self.add_spaced_p2p(node1, P2PInterface())
        announced_before = len(watcher.psgt_invs)
        bad_peer.send_message(msg_psgt(b"not a psgt"))
        corrupted = bytearray(wire)
        corrupted[len(corrupted) // 2] ^= 0x01
        bad_peer.send_message(msg_psgt(bytes(corrupted)))
        bad_peer.sync_with_ping()
        time.sleep(1)  # allow any (wrong) relay to arrive
        assert_equal(len(watcher.psgt_invs), announced_before)
        assert_equal(node1.getbestblockhash(), node0.getbestblockhash())
        self.log.info("garbage/corrupted psgt messages rejected without relay")


if __name__ == "__main__":
    P2PPsgtRelayTest().main()
