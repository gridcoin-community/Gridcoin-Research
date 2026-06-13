#!/usr/bin/env python3
# Copyright (c) 2010 ArtForz -- public domain half-a-node
# Copyright (c) 2012 Jeff Garzik
# Copyright (c) 2010-2021 The Bitcoin Core developers
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Test framework P2P (mininode) connection layer for Gridcoin.

Ported from Bitcoin Core v0.21.2 `test/functional/test_framework/p2p.py`,
trimmed of SegWit/compact-block handling and adapted to Gridcoin's message
framing and command aliases (the version message arrives as `aries`; `addr`
and `block` have legacy `gridaddr`/`encrypt` aliases). Gridcoin-only commands
(`scraperindex`, `part`) and any other unrecognized command are ignored.

Public surface used by the framework (test_node.py / test scripts):
  - P2PConnection / P2PInterface / P2PDataStore
  - NetworkThread (replaces the Phase 1 no-op stub)
  - peer_connect(), is_connected, wait_until(), wait_for_verack(),
    sync_with_ping(), peer_disconnect(), send_message()
"""
import asyncio
from collections import defaultdict
import logging
import struct
import threading

from .messages import (
    CBlockHeader,
    MAGIC_BYTES,
    MAX_LOCATOR_SZ,
    MESSAGEMAP,
    NODE_NETWORK,
    msg_block,
    msg_headers,
    msg_ping,
    msg_pong,
    msg_tx,
    msg_verack,
    msg_version,
    sha256,
    MSG_BLOCK,
    MSG_TX,
)
from .util import wait_until_helper

logger = logging.getLogger("TestFramework.p2p")

# The lock held while a callback runs / while a test inspects received state.
p2p_lock = threading.Lock()


class P2PConnection(asyncio.Protocol):
    """Low-level socket + framing for a single connection to gridcoinresearchd.

    This class handles the wire framing (magic / command / length / checksum)
    and message (de)serialization, then hands parsed messages to on_message().
    """

    def __init__(self):
        self._transport = None

    @property
    def is_connected(self):
        return self._transport is not None

    def peer_connect_helper(self, dstaddr, dstport, net, timeout_factor):
        assert not self.is_connected
        self.timeout_factor = timeout_factor
        self.dstaddr = dstaddr
        self.dstport = dstport
        self.recvbuf = b""
        if net not in MAGIC_BYTES:
            raise ValueError("unknown chain/net %r" % net)
        self.magic_bytes = MAGIC_BYTES[net]

    def peer_connect(self, dstaddr, dstport, *, net, timeout_factor=1):
        self.peer_connect_helper(dstaddr, dstport, net, timeout_factor)
        loop = NetworkThread.network_event_loop
        logger.debug('Connecting to Gridcoin Node: %s:%d' % (self.dstaddr, self.dstport))
        coroutine = loop.create_connection(lambda: self, host=self.dstaddr, port=self.dstport)
        return lambda: loop.call_soon_threadsafe(loop.create_task, coroutine)

    def peer_disconnect(self):
        # Connection could have already been closed by other end.
        NetworkThread.network_event_loop.call_soon_threadsafe(
            lambda: self._transport and self._transport.abort())

    # Connection and disconnection methods

    def connection_made(self, transport):
        """asyncio callback when a connection is opened."""
        assert not self._transport
        logger.debug("Connected & Listening: %s:%d" % (self.dstaddr, self.dstport))
        self._transport = transport
        if self.on_connection_send_msg:
            self.send_message(self.on_connection_send_msg)
            self.on_connection_send_msg = None  # Never used again
        self.on_open()

    def connection_lost(self, exc):
        """asyncio callback when a connection is closed."""
        if exc:
            logger.warning("Connection lost to %s:%d due to %s" % (self.dstaddr, self.dstport, exc))
        else:
            logger.debug("Closed connection to: %s:%d" % (self.dstaddr, self.dstport))
        self._transport = None
        self.recvbuf = b""
        self.on_close()

    # Socket read methods

    def data_received(self, t):
        """asyncio callback when data is read from the socket."""
        if len(t) > 0:
            self.recvbuf += t
            self._on_data()

    def _on_data(self):
        """Try to read P2P messages from the recv buffer.

        This method reads data from the buffer in a loop. It deserializes,
        parses and verifies the P2P header, then passes the P2P payload to
        the on_message callback for processing."""
        try:
            while True:
                if len(self.recvbuf) < 4:
                    return
                if self.recvbuf[:4] != self.magic_bytes:
                    raise ValueError("magic bytes mismatch: {!r} != {!r}".format(self.magic_bytes, self.recvbuf))
                if len(self.recvbuf) < 4 + 12 + 4 + 4:
                    return
                command = self.recvbuf[4:4 + 12].split(b"\x00", 1)[0]
                msglen = struct.unpack("<i", self.recvbuf[4 + 12:4 + 12 + 4])[0]
                checksum = self.recvbuf[4 + 12 + 4:4 + 12 + 4 + 4]
                if len(self.recvbuf) < 4 + 12 + 4 + 4 + msglen:
                    return
                msg = self.recvbuf[4 + 12 + 4 + 4:4 + 12 + 4 + 4 + msglen]
                th = sha256(msg)
                h = sha256(th)
                if checksum != h[:4]:
                    raise ValueError("got bad checksum " + repr(self.recvbuf))
                self.recvbuf = self.recvbuf[4 + 12 + 4 + 4 + msglen:]
                if command not in MESSAGEMAP:
                    # Gridcoin-specific (scraperindex/part) or otherwise unknown
                    # command. The framework peer has no use for these; skip.
                    logger.debug("Received unsupported command from %s:%d: %r" % (self.dstaddr, self.dstport, command))
                    continue
                f = _BytesReader(msg)
                t = MESSAGEMAP[command]()
                t.deserialize(f)
                self._log_message("receive", t)
                self.on_message(t)
        except Exception as e:
            logger.exception('Error reading message: ' + repr(e))
            raise

    def on_message(self, message):
        """Callback for processing a recv'd P2P message.  Subclasses override."""
        raise NotImplementedError

    # Socket write methods

    def send_message(self, message):
        """Send a P2P message over the socket.

        This method takes a P2P payload, builds the P2P header and adds the
        message to the send buffer to be sent over the socket."""
        tmsg = self.build_message(message)
        self._log_message("send", message)
        return self.send_raw_message(tmsg)

    def send_raw_message(self, raw_message_bytes):
        if not self.is_connected:
            raise IOError('Not connected')

        def maybe_write():
            if not self._transport:
                return
            if self._transport.is_closing():
                return
            self._transport.write(raw_message_bytes)
        NetworkThread.network_event_loop.call_soon_threadsafe(maybe_write)

    # Class utility methods

    def build_message(self, message):
        """Build a serialized P2P message from a message object."""
        command = message.command
        data = message.serialize()
        tmsg = self.magic_bytes
        tmsg += command
        tmsg += b"\x00" * (12 - len(command))
        tmsg += struct.pack("<I", len(data))
        th = sha256(data)
        h = sha256(th)
        tmsg += h[:4]
        tmsg += data
        return tmsg

    def _log_message(self, direction, msg):
        """Log a message being sent or received over the connection."""
        if direction == "send":
            log_message = "Send message to "
        elif direction == "receive":
            log_message = "Received message from "
        log_message += "%s:%d: %s" % (self.dstaddr, self.dstport, repr(msg)[:500])
        if len(log_message) > 500:
            log_message += "... (msg truncated)"
        logger.debug(log_message)


class P2PInterface(P2PConnection):
    """A high-level P2P interface for communicating with a Gridcoin node.

    This class provides high-level callbacks (on_version, on_ping, ...) and
    state tracking (message_count, last_message) on top of P2PConnection."""

    def __init__(self):
        super().__init__()
        # Track number of messages of each type received.
        self.message_count = defaultdict(int)
        # Track the most recent message of each type.
        self.last_message = {}
        # A count of the number of ping messages we've sent to the node.
        self.ping_counter = 1
        # The network services received from the peer.
        self.nServices = 0
        self.on_connection_send_msg = None

    def peer_connect_send_version(self):
        # Send a version message on connection.
        vt = msg_version()
        vt.nServices = NODE_NETWORK
        vt.addrTo.ip = self.dstaddr
        vt.addrTo.port = self.dstport
        vt.addrFrom.ip = "0.0.0.0"
        vt.addrFrom.port = 0
        self.on_connection_send_msg = vt  # Will be sent in connection_made callback

    def peer_connect(self, *args, send_version=True, **kwargs):
        create_conn = super().peer_connect(*args, **kwargs)
        if send_version:
            self.peer_connect_send_version()
        return create_conn

    # Message receiving methods

    def on_message(self, message):
        """Receive message and dispatch message to appropriate callback.

        We keep a count of how many of each message type has been received
        and the most recent message of each type."""
        with p2p_lock:
            try:
                command = message.command.decode('ascii')
                # Normalize Gridcoin aliases to canonical handler names.
                command = {"aries": "version", "gridaddr": "addr", "encrypt": "block"}.get(command, command)
                self.message_count[command] += 1
                self.last_message[command] = message
                getattr(self, 'on_' + command)(message)
            except Exception:
                print("ERROR delivering %s (%s)" % (repr(message), str(message.command)))
                raise

    # Callback methods. Can be overridden by subclasses in individual test
    # cases to provide custom message handling behaviour.

    def on_open(self):
        pass

    def on_close(self):
        pass

    def on_addr(self, message):
        pass

    def on_block(self, message):
        pass

    def on_getblocks(self, message):
        pass

    def on_getdata(self, message):
        pass

    def on_getheaders(self, message):
        pass

    def on_headers(self, message):
        pass

    def on_mempool(self, message):
        pass

    def on_notfound(self, message):
        pass

    def on_tx(self, message):
        pass

    def on_inv(self, message):
        pass

    def on_verack(self, message):
        pass

    def on_version(self, message):
        # Don't assert on the peer's advertised version; just ack and record services.
        self.send_message(msg_verack())
        self.nServices = message.nServices

    def on_ping(self, message):
        self.send_message(msg_pong(message.nonce))

    def on_pong(self, message):
        pass

    def on_getaddr(self, message):
        pass

    # Connection helper methods

    def wait_until(self, test_function_in, *, timeout=60, check_connected=True):
        def test_function():
            if check_connected:
                assert self.is_connected
            return test_function_in()

        wait_until_helper(test_function, timeout=timeout, lock=p2p_lock, timeout_factor=self.timeout_factor)

    def wait_for_disconnect(self, timeout=60):
        test_function = lambda: not self.is_connected
        self.wait_until(test_function, timeout=timeout, check_connected=False)

    # Message receiving helper methods

    def wait_for_verack(self, timeout=60):
        def test_function():
            return "verack" in self.last_message

        self.wait_until(test_function, timeout=timeout)

    def wait_for_tx(self, txid, timeout=60):
        def test_function():
            if not self.last_message.get('tx'):
                return False
            return self.last_message['tx'].tx.rehash() == txid

        self.wait_until(test_function, timeout=timeout)

    def wait_for_block(self, blockhash, timeout=60):
        def test_function():
            return self.last_message.get("block") and self.last_message["block"].block.rehash() == blockhash

        self.wait_until(test_function, timeout=timeout)

    def wait_for_getdata(self, hash_list, timeout=60):
        def test_function():
            last_data = self.last_message.get("getdata")
            if not last_data:
                return False
            return [x.hash for x in last_data.inv] == hash_list

        self.wait_until(test_function, timeout=timeout)

    def wait_for_getheaders(self, timeout=60):
        def test_function():
            return self.last_message.get("getheaders")

        self.wait_until(test_function, timeout=timeout)

    def wait_for_inv(self, expected_inv, timeout=60):
        if len(expected_inv) > 1:
            raise NotImplementedError("wait_for_inv() will only verify a single inv")

        def test_function():
            return self.last_message.get("inv") and \
                self.last_message["inv"].inv[0].type == expected_inv[0].type and \
                self.last_message["inv"].inv[0].hash == expected_inv[0].hash

        self.wait_until(test_function, timeout=timeout)

    # Message sending helper functions

    def send_and_ping(self, message, timeout=60):
        self.send_message(message)
        self.sync_with_ping(timeout=timeout)

    def sync_with_ping(self, timeout=60):
        """Ensure ProcessMessages and SendMessages is called on this connection."""
        # Sending two pings back-to-back, requesting the highest possible nonce.
        self.send_message(msg_ping(nonce=self.ping_counter))

        def test_function():
            return self.last_message.get("pong") and self.last_message["pong"].nonce == self.ping_counter

        self.wait_until(test_function, timeout=timeout)
        self.ping_counter += 1


class NetworkThread(threading.Thread):
    """Runs the asyncio event loop that drives all P2PConnection sockets.

    Replaces the Phase 1 no-op stub. start()/close() are called by the test
    framework's setup()/shutdown()."""

    network_event_loop = None

    def __init__(self):
        super().__init__(name="NetworkThread")
        # There is only one event loop and no more than one thread must be
        # created.
        assert NetworkThread.network_event_loop is None
        NetworkThread.network_event_loop = asyncio.new_event_loop()

    def run(self):
        """Start the network thread."""
        self.network_event_loop.run_forever()

    def close(self, timeout=10):
        """Close the connections and network event loop."""
        self.network_event_loop.call_soon_threadsafe(self.network_event_loop.stop)
        wait_until_helper(lambda: not self.network_event_loop.is_running(), timeout=timeout)
        self.network_event_loop.close()
        self.join(timeout)
        # Safe to remove event loop.
        NetworkThread.network_event_loop = None


class P2PDataStore(P2PInterface):
    """A P2P interface that keeps a store of blocks and transactions.

    Answers the node's getdata/getheaders for any object in the store, and
    provides helpers to push blocks/txs to the node and assert acceptance."""

    def __init__(self):
        super().__init__()
        # store of blocks. key is block hash, value is a CBlock object
        self.block_store = {}
        self.last_block_hash = ''
        # store of txs. key is txid, value is a CTransaction object
        self.tx_store = {}
        self.getdata_requests = []

    def on_getdata(self, message):
        """Check for the tx/block in our stores and if found, reply with it."""
        for inv in message.inv:
            self.getdata_requests.append(inv.hash)
            if (inv.type & MSG_TX) and inv.hash in self.tx_store:
                self.send_message(msg_tx(self.tx_store[inv.hash]))
            elif (inv.type & MSG_BLOCK) and inv.hash in self.block_store:
                self.send_message(msg_block(self.block_store[inv.hash]))

    def on_getheaders(self, message):
        """Search back through our block store for the locator, and reply with a headers message if found."""
        locator, hash_stop = message.locator, message.hashstop
        # Assume that the most recent block added is the tip
        if not self.block_store:
            return
        headers_list = [self.block_store[self.last_block_hash]]
        while headers_list[-1].sha256 not in locator.vHave:
            # Walk back through the block store, adding headers to headers_list.
            prev_block_hash = headers_list[-1].hashPrevBlock
            if prev_block_hash in self.block_store:
                prev_block_header = CBlockHeader(self.block_store[prev_block_hash])
                headers_list.append(prev_block_header)
                if prev_block_header.sha256 == hash_stop:
                    break
            else:
                break
        # Truncate the list if there are too many headers
        headers_list = headers_list[:-MAX_LOCATOR_SZ - 1:-1] if len(headers_list) else headers_list
        response = msg_headers([CBlockHeader(b) for b in headers_list])
        if response is not None:
            self.send_message(response)

    def send_blocks_and_test(self, blocks, node, *, success=True, force_send=False, timeout=60):
        """Send blocks to the node and check that the node's tip reflects acceptance.

        - add all blocks to our block_store
        - send an inv (or the blocks directly if force_send) for the last block
        - if success: assert the node's tip is the last block
        - if not success: assert the node's tip is NOT the last block
        """
        with p2p_lock:
            for block in blocks:
                self.block_store[block.sha256] = block
                self.last_block_hash = block.sha256

        if force_send:
            for b in blocks:
                self.send_message(msg_block(block=b))
        else:
            self.send_message(msg_headers([CBlockHeader(block) for block in blocks]))
            self.wait_until(
                lambda: blocks[-1].sha256 in self.getdata_requests,
                timeout=timeout,
                check_connected=success)

        self.sync_with_ping(timeout=timeout)

        if success:
            self.wait_until(lambda: node.getbestblockhash() == blocks[-1].hash, timeout=timeout)
        else:
            assert node.getbestblockhash() != blocks[-1].hash

    def send_txs_and_test(self, txs, node, *, success=True, timeout=60):
        """Send txs to the node and check that they are accepted into the mempool (or not)."""
        with p2p_lock:
            for tx in txs:
                self.tx_store[tx.sha256] = tx

        for tx in txs:
            self.send_message(msg_tx(tx))

        self.sync_with_ping(timeout=timeout)

        raw_mempool = node.getrawmempool()
        if success:
            for tx in txs:
                assert tx.hash in raw_mempool, "{} not found in mempool".format(tx.hash)
        else:
            for tx in txs:
                assert tx.hash not in raw_mempool, "{} should not be in mempool".format(tx.hash)


class _BytesReader:
    """Minimal file-like wrapper over a bytes payload for the deserializers."""

    def __init__(self, data):
        self._data = data
        self._pos = 0

    def read(self, n=-1):
        if n is None or n < 0:
            n = len(self._data) - self._pos
        chunk = self._data[self._pos:self._pos + n]
        self._pos += len(chunk)
        return chunk

    def seek(self, offset, whence=0):
        if whence == 0:
            self._pos = offset
        elif whence == 1:
            self._pos += offset
        elif whence == 2:
            self._pos = len(self._data) + offset
        return self._pos
