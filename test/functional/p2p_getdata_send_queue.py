#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""A getdata is served as the peer's send queue has room.

The node serves the entries of a getdata while the peer's send queue is under
-maxsendbuffer. The entries it has not served stay on the peer and are served
on later passes of the message handler, and nothing else is read from that peer
until they are gone. This test runs the node with a small -maxsendbuffer and
checks that:

  - a peer that reads its replies gets every one, in the order it asked, and
    the reply to a message it sent after the getdata comes after them;
  - while a peer that has stopped reading has entries held, the node answers
    another peer and its RPC;
  - when that peer reads again it gets every reply, in the order it asked,
    and then the reply to the message it sent after the getdata;
  - a peer that disconnects with entries held leaves the node serving others.
"""

from io import BytesIO

from test_framework.messages import (
    CInv,
    CTransaction,
    MSG_BLOCK,
    MSG_TX,
    msg_getdata,
    msg_ping,
)
from test_framework.p2p import NetworkThread, P2PInterface, p2p_lock
from test_framework.test_framework import GridcoinTestFramework
from test_framework.util import assert_equal

HELD_MSG = "entries held for"

# Outputs of the transaction that makes one block large, and how many times
# the large request asks for that block and the ordinary blocks after it.
# Together the replies are far more than the socket buffers between the node
# and a peer that does not read can take.
LARGE_TX_OUTPUTS = 200
LARGE_REQUEST_ROUNDS = 4000


class RecordingPeer(P2PInterface):
    """Keeps the block, tx and pong messages it receives, in order."""

    def __init__(self):
        super().__init__()
        self.received = []

    def on_block(self, message):
        message.block.rehash()
        self.received.append(("block", message.block.sha256))

    def on_tx(self, message):
        message.tx.rehash()
        self.received.append(("tx", message.tx.sha256))

    def on_pong(self, message):
        self.received.append(("pong", message.nonce))

    def has_pong(self, nonce):
        """For wait_until(), which calls it with p2p_lock held."""
        return ("pong", nonce) in self.received

    def stop_reading(self):
        NetworkThread.network_event_loop.call_soon_threadsafe(self._transport.pause_reading)

    def start_reading(self):
        NetworkThread.network_event_loop.call_soon_threadsafe(self._transport.resume_reading)


class P2PGetDataSendQueueTest(GridcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.chain = "regtest"
        self.setup_clean_chain = True
        # -maxsendbuffer is in units of 1000 bytes.
        self.extra_args = [["-staking=0", "-maxsendbuffer=10"]]

    def setup_network(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def oldest_coinstake_utxo(self, node):
        height = node.getblockcount()
        coinstakes = [u for u in node.listunspent(0) if u["confirmations"] <= height]
        assert coinstakes, "no coinstake output to spend"
        return max(coinstakes, key=lambda u: u["confirmations"])

    def signed_tx(self, node, utxo, outputs):
        raw = node.createrawtransaction([{"txid": utxo["txid"], "vout": utxo["vout"]}], outputs)
        signed = node.signrawtransactionwithwallet(raw)
        assert signed.get("complete"), signed
        return signed["hex"]

    def run_test(self):
        node = self.nodes[0]
        sink = node.getnewaddress()

        # Coinstake outputs are spent, as the blocks are staked with the
        # premine outputs.
        node.generatetoaddress(10, sink)

        # A block made large by one transaction with many outputs.
        large_utxo = self.oldest_coinstake_utxo(node)
        each = round((float(large_utxo["amount"]) - 10.0) / LARGE_TX_OUTPUTS, 4)
        outputs = {node.getnewaddress(): each for _ in range(LARGE_TX_OUTPUTS)}
        node.sendrawtransaction(self.signed_tx(node, large_utxo, outputs))
        # A transaction is mined once the clock is past its own time slot.
        self.advance_to_next_stake_slot()
        large_hash = node.generatetoaddress(1, sink)[0]
        large_block = int(large_hash, 16)
        assert_equal(len(node.getrawmempool()), 0)

        # Some ordinary blocks, and a transaction left in the mempool.
        small_blocks = [int(h, 16) for h in node.generatetoaddress(5, sink)]

        # Chosen after the last block: staking a block can spend an output
        # listed before it.
        small_utxo = self.oldest_coinstake_utxo(node)
        mempool_hex = self.signed_tx(node, small_utxo, {sink: float(small_utxo["amount"]) - 1.0})
        mempool_tx = CTransaction()
        mempool_tx.deserialize(BytesIO(bytes.fromhex(mempool_hex)))
        mempool_tx.rehash()
        node.sendrawtransaction(mempool_hex)
        height = node.getblockcount()

        unknown = 0x1234567890abcdef

        # The large request: distinct blocks in a fixed order, many times over.
        large_request_blocks = ([large_block] + small_blocks) * LARGE_REQUEST_ROUNDS
        large_request = msg_getdata([CInv(MSG_BLOCK, block) for block in large_request_blocks])

        self.log.info("A peer that reads gets its replies in the order asked, then the pong")
        reader = node.add_p2p_connection(RecordingPeer())

        request = []
        expected = []
        for _ in range(20):
            for block in [large_block] + small_blocks:
                request.append(CInv(MSG_BLOCK, block))
                expected.append(("block", block))
            request.append(CInv(MSG_TX, mempool_tx.sha256))
            expected.append(("tx", mempool_tx.sha256))
            # An entry the node does not have is passed over.
            request.append(CInv(MSG_BLOCK, unknown))
            request.append(CInv(MSG_TX, unknown))

        with p2p_lock:
            reader.received.clear()
        reader.send_message(msg_getdata(request))
        reader.send_message(msg_ping(nonce=1001))
        reader.wait_until(lambda: reader.has_pong(1001), timeout=60)
        with p2p_lock:
            assert_equal(reader.received, expected + [("pong", 1001)])

        self.log.info("A peer that stops reading has entries held; the node answers another peer")
        stalled = self.add_p2p_connection_spaced(node, RecordingPeer())
        stalled.stop_reading()
        # The handshake's own ping has been answered by now.
        with p2p_lock:
            stalled.received.clear()

        with node.assert_debug_log([HELD_MSG], timeout=60):
            stalled.send_message(large_request)
            stalled.send_message(msg_ping(nonce=2002))

        with p2p_lock:
            reader.received.clear()
        reader.send_message(msg_getdata([CInv(MSG_BLOCK, small_blocks[0])]))
        reader.send_message(msg_ping(nonce=1002))
        reader.wait_until(lambda: reader.has_pong(1002), timeout=30)
        with p2p_lock:
            assert_equal(reader.received, [("block", small_blocks[0]), ("pong", 1002)])
        assert_equal(node.getblockcount(), height)

        self.log.info("When it reads again it gets the rest in order, then the pong")
        stalled.start_reading()
        stalled.wait_until(lambda: stalled.has_pong(2002), timeout=240)
        with p2p_lock:
            assert_equal(len(stalled.received), len(large_request_blocks) + 1)
            # Compared as booleans: a failed comparison of the lists themselves
            # would print both.
            assert_equal(stalled.received[:-1] == [("block", block) for block in large_request_blocks], True)
            assert_equal(stalled.received[-1], ("pong", 2002))

        self.log.info("A peer that disconnects with entries held leaves the node serving others")
        leaver = self.add_p2p_connection_spaced(node, RecordingPeer())
        leaver.stop_reading()

        with node.assert_debug_log([HELD_MSG], timeout=60):
            leaver.send_message(large_request)

        peers_before = len(node.getpeerinfo())
        leaver.peer_disconnect()
        leaver.wait_for_disconnect()
        self.wait_until(lambda: len(node.getpeerinfo()) == peers_before - 1, timeout=30)

        with p2p_lock:
            reader.received.clear()
        reader.send_message(msg_getdata([CInv(MSG_BLOCK, large_block)]))
        reader.send_message(msg_ping(nonce=1003))
        reader.wait_until(lambda: reader.has_pong(1003), timeout=30)
        with p2p_lock:
            assert_equal(reader.received, [("block", large_block), ("pong", 1003)])
        assert_equal(node.getblockcount(), height)

        node.disconnect_p2ps()


if __name__ == "__main__":
    P2PGetDataSendQueueTest().main()
