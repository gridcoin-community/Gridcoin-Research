#!/usr/bin/env python3
# Copyright (c) 2010 ArtForz -- public domain half-a-node
# Copyright (c) 2012 Jeff Garzik
# Copyright (c) 2010-2021 The Bitcoin Core developers
# Copyright (c) 2014-2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Gridcoin P2P wire-protocol primitives for the functional-test framework.

Ported from Bitcoin Core v0.21.2 `test/functional/test_framework/messages.py`
and adapted to Gridcoin's proof-of-stake wire format. Differences from upstream
(all verified against the C++ source — file:line cited):

- CTransaction carries `nTime` (uint32) immediately after `nVersion`, and ends
  with version-gated `vContracts` (nVersion>=2) or `hashBoinc` (nVersion==1)
  instead of any witness data. There is no SegWit marker/flag/witness — ever.
  (src/primitives/transaction.h:296-309)
- CBlockHeader serializes `nNonce` only when `nVersion <= 10`; regtest blocks
  are v14, so nNonce is absent on the wire. The block id is the double-SHA256
  of the serialized header. (src/main.h:304-320, 377-384)
- CBlock appends `vchBlockSig` (the PoS block signature) after `vtx`; it does
  not participate in the block hash. (src/main.h:439-452)
- The "version" message is sent on the wire as command `aries`; `addr`/`block`
  have legacy aliases `gridaddr`/`encrypt`. We map all aliases on receive.
  (src/protocol.cpp:33-39, src/net.cpp:518)
- The version payload has NO `fRelay` byte and skips the pre-180324 legacy boinc
  fields (we advertise 180329). Both the top-level `nServices` and the `nServices`
  *inside* a CAddress are little-endian uint64 (CustomUintFormatter<8> defaults to
  BigEndian=false; src/serialize.h:587). Only the CAddress IP and port are
  big-endian (network order). The version-message CAddresses include `nTime`
  because the send stream
  serializes at INIT_PROTO_VERSION and Gridcoin gates CAddress.nTime on
  `>= INIT_PROTO_VERSION`. (src/net.cpp:486-527, src/net.h:327, src/protocol.h:78-91)
"""
import copy
import hashlib
import struct
import time

MAX_LOCATOR_SZ = 101
MAX_BLOCK_BASE_SIZE = 1000000

COIN = 100000000  # 1 GRC in halflings

# Protocol constants (src/version.h)
MY_VERSION = 180329          # PROTOCOL_VERSION
MIN_PEER_PROTO_VERSION = 180327
INIT_PROTO_VERSION = 180275

MY_SUBVERSION = "/python-mininode-tester:0.0.3/"
MY_RELAY = 1  # placeholder; Gridcoin's version message has no relay field

NODE_NETWORK = (1 << 0)

# Per-network message-start magic bytes (src/chainparams.cpp).
MAGIC_BYTES = {
    "main": b"\x70\x35\x22\x05",
    "test": b"\xcd\xf2\xc0\xef",
    "regtest": b"\xfa\xbf\xb5\xda",
}

# Inv/getdata type codes (src/protocol.h GetDataMsg). No witness variants.
MSG_TX = 1
MSG_BLOCK = 2


# Serialization/deserialization tools
def sha256(s):
    return hashlib.new('sha256', s).digest()


def hash256(s):
    return sha256(sha256(s))


def ser_compact_size(l):
    r = b""
    if l < 253:
        r = struct.pack("B", l)
    elif l < 0x10000:
        r = struct.pack("<BH", 253, l)
    elif l < 0x100000000:
        r = struct.pack("<BI", 254, l)
    else:
        r = struct.pack("<BQ", 255, l)
    return r


def deser_compact_size(f):
    nit = struct.unpack("<B", f.read(1))[0]
    if nit == 253:
        nit = struct.unpack("<H", f.read(2))[0]
    elif nit == 254:
        nit = struct.unpack("<I", f.read(4))[0]
    elif nit == 255:
        nit = struct.unpack("<Q", f.read(8))[0]
    return nit


def deser_string(f):
    nit = deser_compact_size(f)
    return f.read(nit)


def ser_string(s):
    return ser_compact_size(len(s)) + s


def deser_uint256(f):
    r = 0
    for i in range(8):
        t = struct.unpack("<I", f.read(4))[0]
        r += t << (i * 32)
    return r


def ser_uint256(u):
    rs = b""
    for _ in range(8):
        rs += struct.pack("<I", u & 0xFFFFFFFF)
        u >>= 32
    return rs


def uint256_from_str(s):
    r = 0
    t = struct.unpack("<IIIIIIII", s[:32])
    for i in range(8):
        r += t[i] << (i * 32)
    return r


def deser_vector(f, c):
    nit = deser_compact_size(f)
    r = []
    for _ in range(nit):
        t = c()
        t.deserialize(f)
        r.append(t)
    return r


def ser_vector(l):
    r = ser_compact_size(len(l))
    for i in l:
        r += i.serialize()
    return r


def deser_uint256_vector(f):
    nit = deser_compact_size(f)
    r = []
    for _ in range(nit):
        r.append(deser_uint256(f))
    return r


def ser_uint256_vector(l):
    r = ser_compact_size(len(l))
    for i in l:
        r += ser_uint256(i)
    return r


# Objects that map to gridcoinresearchd objects, which can be serialized/deserialized

class CAddress:
    """A network address.

    Gridcoin gates `CAddress.nTime` on `nVersion >= INIT_PROTO_VERSION` and uses
    a little-endian uint64 for `nServices` (CustomUintFormatter<8> defaults to
    BigEndian=false); only the IP and port are big-endian. The version message
    serializes its embedded addresses at INIT_PROTO_VERSION, so they DO carry
    nTime — pass with_time=True there (the default).
    """

    def __init__(self):
        self.time = 0
        self.nServices = NODE_NETWORK
        self.ip = "0.0.0.0"
        self.port = 0

    def deserialize(self, f, *, with_time=True):
        if with_time:
            # nTime is a plain little-endian uint32 (READWRITE(nTime)).
            self.time = struct.unpack("<I", f.read(4))[0]
        # nServices: little-endian uint64 (CustomUintFormatter<8>, default BigEndian=false).
        self.nServices = struct.unpack("<Q", f.read(8))[0]
        f.read(12)  # IPv4-mapped-IPv6 prefix (10 zero bytes + 0xffff)
        self.ip = struct.unpack(">I", f.read(4))[0]
        # port: big-endian uint16 (WrapBigEndian(port)).
        self.port = struct.unpack(">H", f.read(2))[0]

    def serialize(self, *, with_time=True):
        r = b""
        if with_time:
            r += struct.pack("<I", self.time)
        r += struct.pack("<Q", self.nServices)
        r += b"\x00" * 10 + b"\xff\xff"
        r += struct.pack(">I", self.ip if isinstance(self.ip, int) else _ip_to_int(self.ip))
        r += struct.pack(">H", self.port)
        return r

    def __repr__(self):
        return "CAddress(nServices=%i ip=%s port=%i)" % (self.nServices, self.ip, self.port)


def _ip_to_int(ip):
    return struct.unpack(">I", bytes(int(x) for x in ip.split(".")))[0]


class CInv:
    typemap = {0: "Error", MSG_TX: "TX", MSG_BLOCK: "Block"}

    def __init__(self, t=0, h=0):
        self.type = t
        self.hash = h

    def deserialize(self, f):
        self.type = struct.unpack("<I", f.read(4))[0]
        self.hash = deser_uint256(f)

    def serialize(self):
        r = b""
        r += struct.pack("<I", self.type)
        r += ser_uint256(self.hash)
        return r

    def __repr__(self):
        return "CInv(type=%s hash=%064x)" % (self.typemap.get(self.type, "%d" % self.type), self.hash)


class CBlockLocator:
    def __init__(self):
        self.nVersion = MY_VERSION
        self.vHave = []

    def deserialize(self, f):
        self.nVersion = struct.unpack("<i", f.read(4))[0]
        self.vHave = deser_uint256_vector(f)

    def serialize(self):
        r = b""
        r += struct.pack("<i", self.nVersion)
        r += ser_uint256_vector(self.vHave)
        return r

    def __repr__(self):
        return "CBlockLocator(nVersion=%i vHave=%s)" % (self.nVersion, repr(self.vHave))


class COutPoint:
    def __init__(self, hash=0, n=0):
        self.hash = hash
        self.n = n

    def deserialize(self, f):
        self.hash = deser_uint256(f)
        self.n = struct.unpack("<I", f.read(4))[0]

    def serialize(self):
        r = b""
        r += ser_uint256(self.hash)
        r += struct.pack("<I", self.n)
        return r

    def __repr__(self):
        return "COutPoint(hash=%064x n=%i)" % (self.hash, self.n)


class CTxIn:
    def __init__(self, outpoint=None, scriptSig=b"", nSequence=0):
        if outpoint is None:
            self.prevout = COutPoint()
        else:
            self.prevout = outpoint
        self.scriptSig = scriptSig
        self.nSequence = nSequence

    def deserialize(self, f):
        self.prevout = COutPoint()
        self.prevout.deserialize(f)
        self.scriptSig = deser_string(f)
        self.nSequence = struct.unpack("<I", f.read(4))[0]

    def serialize(self):
        r = b""
        r += self.prevout.serialize()
        r += ser_string(self.scriptSig)
        r += struct.pack("<I", self.nSequence)
        return r

    def __repr__(self):
        return "CTxIn(prevout=%s scriptSig=%s nSequence=%i)" \
            % (repr(self.prevout), self.scriptSig.hex(), self.nSequence)


class CTxOut:
    def __init__(self, nValue=0, scriptPubKey=b""):
        self.nValue = nValue
        self.scriptPubKey = scriptPubKey

    def deserialize(self, f):
        self.nValue = struct.unpack("<q", f.read(8))[0]
        self.scriptPubKey = deser_string(f)

    def serialize(self):
        r = b""
        r += struct.pack("<q", self.nValue)
        r += ser_string(self.scriptPubKey)
        return r

    def __repr__(self):
        return "CTxOut(nValue=%i.%08i scriptPubKey=%s)" \
            % (self.nValue // COIN, self.nValue % COIN, self.scriptPubKey.hex())


class ClaimPayload:
    """The body of a `claim` contract (GRC::Claim, src/gridcoin/claim.h).

    Only the fields actually emitted on regtest are modeled. A researcher
    (CPID) claim or a valid quorum hash (which would pull in a full superblock)
    is not produced on regtest and raises rather than mis-parsing.
    """

    # MiningId::Kind (src/gridcoin/cpid.h)
    MINING_INVALID = 0
    MINING_NONCRUNCHER = 1
    MINING_CPID = 2

    def deserialize(self, f):
        self.version = struct.unpack("<I", f.read(4))[0]
        self.mining_kind = struct.unpack("<B", f.read(1))[0]
        self.cpid = b""
        if self.mining_kind == self.MINING_CPID:
            self.cpid = f.read(16)
        self.client_version = deser_string(f)
        self.organization = deser_string(f)
        self.block_subsidy = struct.unpack("<q", f.read(8))[0]
        self.research_subsidy = None
        self.signature = b""
        if self.mining_kind == self.MINING_CPID:
            self.research_subsidy = struct.unpack("<q", f.read(8))[0]
            self.signature = deser_string(f)
        self.quorum_kind = struct.unpack("<B", f.read(1))[0]
        if self.quorum_kind != 0:  # QuorumHash::Kind::INVALID == 0
            raise NotImplementedError("claim with a valid quorum hash / superblock is not modeled")
        self.mrc_tx_map = []  # std::map<Cpid(16), uint256(32)>
        if self.version >= 4:
            n = deser_compact_size(f)
            for _ in range(n):
                self.mrc_tx_map.append((f.read(16), f.read(32)))

    def serialize(self):
        r = struct.pack("<I", self.version)
        r += struct.pack("<B", self.mining_kind)
        if self.mining_kind == self.MINING_CPID:
            r += self.cpid
        r += ser_string(self.client_version)
        r += ser_string(self.organization)
        r += struct.pack("<q", self.block_subsidy)
        if self.mining_kind == self.MINING_CPID:
            r += struct.pack("<q", self.research_subsidy)
            r += ser_string(self.signature)
        r += struct.pack("<B", self.quorum_kind)
        if self.version >= 4:
            r += ser_compact_size(len(self.mrc_tx_map))
            for cpid, txhash in self.mrc_tx_map:
                r += cpid + txhash
        return r

    def __repr__(self):
        return "ClaimPayload(version=%i mining_kind=%i client_version=%r block_subsidy=%i)" \
            % (self.version, self.mining_kind, self.client_version, self.block_subsidy)


# Contract m_type values (src/gridcoin/contract/contract.h Contract::Type).
CONTRACT_TYPE_CLAIM = 2


class GridcoinContract:
    """A Gridcoin transaction contract (GRC::Contract).

    Layout: m_version(u32), m_type(u8), m_action(u8), m_body. Only the `claim`
    body (the one a regtest coinstake/coinbase carries) is modeled; other types
    raise rather than silently desync the stream.
    """

    def deserialize(self, f):
        self.version = struct.unpack("<I", f.read(4))[0]
        self.ctype = struct.unpack("<B", f.read(1))[0]
        self.action = struct.unpack("<B", f.read(1))[0]
        if self.ctype == CONTRACT_TYPE_CLAIM:
            self.body = ClaimPayload()
            self.body.deserialize(f)
        else:
            raise NotImplementedError(
                "contract type %d is not modeled by the test framework (only claim=%d)"
                % (self.ctype, CONTRACT_TYPE_CLAIM))

    def serialize(self):
        r = struct.pack("<I", self.version)
        r += struct.pack("<B", self.ctype)
        r += struct.pack("<B", self.action)
        r += self.body.serialize()
        return r

    def __repr__(self):
        return "GridcoinContract(version=%i type=%i action=%i body=%s)" \
            % (self.version, self.ctype, self.action, repr(self.body))


class CTransaction:
    """A Gridcoin transaction.

    Layout: nVersion(i32), nTime(u32), vin, vout, nLockTime(u32), then for
    nVersion>=2 a vector of contracts (empty for ordinary payments, a `claim`
    contract for coinbase/coinstake), else the legacy hashBoinc string for
    nVersion==1. No witness data.
    """

    def __init__(self, tx=None):
        if tx is None:
            self.nVersion = 1
            self.nTime = 0
            self.vin = []
            self.vout = []
            self.nLockTime = 0
            self.hashBoinc = b""
            self.vContracts = []  # only used for nVersion>=2; empty for payments
            self.sha256 = None
            self.hash = None
        else:
            self.nVersion = tx.nVersion
            self.nTime = tx.nTime
            self.vin = copy.deepcopy(tx.vin)
            self.vout = copy.deepcopy(tx.vout)
            self.nLockTime = tx.nLockTime
            self.hashBoinc = tx.hashBoinc
            self.vContracts = copy.deepcopy(tx.vContracts)
            self.sha256 = tx.sha256
            self.hash = tx.hash

    def deserialize(self, f):
        self.nVersion = struct.unpack("<i", f.read(4))[0]
        self.nTime = struct.unpack("<I", f.read(4))[0]
        self.vin = deser_vector(f, CTxIn)
        self.vout = deser_vector(f, CTxOut)
        self.nLockTime = struct.unpack("<I", f.read(4))[0]
        if self.nVersion >= 2:
            self.vContracts = deser_vector(f, GridcoinContract)
        else:
            self.hashBoinc = deser_string(f)
        self.sha256 = None
        self.hash = None

    def serialize(self):
        r = b""
        r += struct.pack("<i", self.nVersion)
        r += struct.pack("<I", self.nTime)
        r += ser_vector(self.vin)
        r += ser_vector(self.vout)
        r += struct.pack("<I", self.nLockTime)
        if self.nVersion >= 2:
            r += ser_vector(self.vContracts)
        else:
            r += ser_string(self.hashBoinc)
        return r

    def rehash(self):
        self.sha256 = None
        self.calc_sha256()
        return self.hash

    def calc_sha256(self):
        if self.sha256 is None:
            self.sha256 = uint256_from_str(hash256(self.serialize()))
        self.hash = hash256(self.serialize())[::-1].hex()

    def __repr__(self):
        return "CTransaction(nVersion=%i nTime=%i vin=%s vout=%s nLockTime=%i)" \
            % (self.nVersion, self.nTime, repr(self.vin), repr(self.vout), self.nLockTime)


class CBlockHeader:
    """A Gridcoin block header.

    nNonce is serialized only for nVersion <= 10. The block id is the
    double-SHA256 of the serialized header (nVersion >= 7).
    """

    def __init__(self, header=None):
        if header is None:
            self.set_null()
        else:
            self.nVersion = header.nVersion
            self.hashPrevBlock = header.hashPrevBlock
            self.hashMerkleRoot = header.hashMerkleRoot
            self.nTime = header.nTime
            self.nBits = header.nBits
            self.nNonce = header.nNonce
            self.sha256 = header.sha256
            self.hash = header.hash
            self.calc_sha256()

    def set_null(self):
        self.nVersion = 14
        self.hashPrevBlock = 0
        self.hashMerkleRoot = 0
        self.nTime = 0
        self.nBits = 0
        self.nNonce = 0
        self.sha256 = None
        self.hash = None

    def deserialize(self, f):
        self.nVersion = struct.unpack("<i", f.read(4))[0]
        self.hashPrevBlock = deser_uint256(f)
        self.hashMerkleRoot = deser_uint256(f)
        self.nTime = struct.unpack("<I", f.read(4))[0]
        self.nBits = struct.unpack("<I", f.read(4))[0]
        if self.nVersion <= 10:
            self.nNonce = struct.unpack("<I", f.read(4))[0]
        else:
            self.nNonce = 0
        self.sha256 = None
        self.hash = None

    def serialize(self):
        r = b""
        r += struct.pack("<i", self.nVersion)
        r += ser_uint256(self.hashPrevBlock)
        r += ser_uint256(self.hashMerkleRoot)
        r += struct.pack("<I", self.nTime)
        r += struct.pack("<I", self.nBits)
        if self.nVersion <= 10:
            r += struct.pack("<I", self.nNonce)
        return r

    def calc_sha256(self):
        if self.sha256 is None:
            # Always hash the header fields only — call the base serializer
            # explicitly so that for a CBlock instance the vtx/vchBlockSig from
            # the subclass override do not leak into the block id.
            r = CBlockHeader.serialize(self)
            self.sha256 = uint256_from_str(hash256(r))
            self.hash = hash256(r)[::-1].hex()

    def rehash(self):
        self.sha256 = None
        self.calc_sha256()
        return self.sha256

    def __repr__(self):
        return "CBlockHeader(nVersion=%i hashPrevBlock=%064x hashMerkleRoot=%064x nTime=%s nBits=%08x nNonce=%08x)" \
            % (self.nVersion, self.hashPrevBlock, self.hashMerkleRoot,
               time.ctime(self.nTime), self.nBits, self.nNonce)


class CBlock(CBlockHeader):
    """A Gridcoin block: header + vtx + vchBlockSig (PoS signature)."""

    def __init__(self, header=None):
        super().__init__(header)
        self.vtx = []
        self.vchBlockSig = b""

    def deserialize(self, f):
        super().deserialize(f)
        self.vtx = deser_vector(f, CTransaction)
        self.vchBlockSig = deser_string(f)

    def serialize(self):
        r = b""
        r += super().serialize()
        r += ser_vector(self.vtx)
        r += ser_string(self.vchBlockSig)
        return r

    def __repr__(self):
        return "CBlock(nVersion=%i hashPrevBlock=%064x hashMerkleRoot=%064x nTime=%s nBits=%08x nNonce=%08x vtx=%s sig_len=%i)" \
            % (self.nVersion, self.hashPrevBlock, self.hashMerkleRoot,
               time.ctime(self.nTime), self.nBits, self.nNonce, repr(self.vtx), len(self.vchBlockSig))


# Objects that correspond to messages on the wire.
#
# Each message class carries a `command` byte string (the wire command name).
# Gridcoin sends the version message as "aries"; on receive we also accept the
# legacy aliases for addr/block (see MESSAGEMAP / the alias table in p2p.py).

class msg_version:
    __slots__ = ("nVersion", "nServices", "nTime", "addrTo", "addrFrom",
                 "nNonce", "strSubVer", "nStartingHeight")
    command = b"aries"

    def __init__(self):
        self.nVersion = MY_VERSION
        self.nServices = NODE_NETWORK
        self.nTime = int(time.time())
        self.addrTo = CAddress()
        self.addrFrom = CAddress()
        self.nNonce = 0
        self.strSubVer = MY_SUBVERSION
        self.nStartingHeight = -1

    def deserialize(self, f):
        self.nVersion = struct.unpack("<i", f.read(4))[0]
        # Top-level nServices: little-endian uint64.
        self.nServices = struct.unpack("<Q", f.read(8))[0]
        self.nTime = struct.unpack("<q", f.read(8))[0]
        self.addrTo = CAddress()
        self.addrTo.deserialize(f, with_time=True)
        self.addrFrom = CAddress()
        self.addrFrom.deserialize(f, with_time=True)
        self.nNonce = struct.unpack("<Q", f.read(8))[0]
        self.strSubVer = deser_string(f).decode('latin-1')
        if f.read(1) != b"":
            f.seek(-1, 1)
            self.nStartingHeight = struct.unpack("<i", f.read(4))[0]
        else:
            self.nStartingHeight = None
        # No fRelay field in Gridcoin's version message.

    def serialize(self):
        r = b""
        r += struct.pack("<i", self.nVersion)
        r += struct.pack("<Q", self.nServices)
        r += struct.pack("<q", self.nTime)
        r += self.addrTo.serialize(with_time=True)
        r += self.addrFrom.serialize(with_time=True)
        r += struct.pack("<Q", self.nNonce)
        r += ser_string(self.strSubVer.encode('latin-1'))
        r += struct.pack("<i", self.nStartingHeight)
        return r

    def __repr__(self):
        return 'msg_version(nVersion=%i nServices=%i nTime=%s nNonce=%i strSubVer=%s nStartingHeight=%i)' \
            % (self.nVersion, self.nServices, time.ctime(self.nTime), self.nNonce,
               self.strSubVer, self.nStartingHeight)


class msg_verack:
    __slots__ = ()
    command = b"verack"

    def deserialize(self, f):
        pass

    def serialize(self):
        return b""

    def __repr__(self):
        return "msg_verack()"


class msg_addr:
    __slots__ = ("addrs",)
    command = b"addr"

    def __init__(self):
        self.addrs = []

    def deserialize(self, f):
        n = deser_compact_size(f)
        self.addrs = []
        for _ in range(n):
            a = CAddress()
            a.deserialize(f, with_time=True)
            self.addrs.append(a)

    def serialize(self):
        r = ser_compact_size(len(self.addrs))
        for a in self.addrs:
            r += a.serialize(with_time=True)
        return r

    def __repr__(self):
        return "msg_addr(addrs=%s)" % (repr(self.addrs))


class msg_inv:
    __slots__ = ("inv",)
    command = b"inv"

    def __init__(self, inv=None):
        self.inv = inv if inv is not None else []

    def deserialize(self, f):
        self.inv = deser_vector(f, CInv)

    def serialize(self):
        return ser_vector(self.inv)

    def __repr__(self):
        return "msg_inv(inv=%s)" % (repr(self.inv))


class msg_getdata:
    __slots__ = ("inv",)
    command = b"getdata"

    def __init__(self, inv=None):
        self.inv = inv if inv is not None else []

    def deserialize(self, f):
        self.inv = deser_vector(f, CInv)

    def serialize(self):
        return ser_vector(self.inv)

    def __repr__(self):
        return "msg_getdata(inv=%s)" % (repr(self.inv))


class msg_getblocks:
    __slots__ = ("locator", "hashstop")
    command = b"getblocks"

    def __init__(self):
        self.locator = CBlockLocator()
        self.hashstop = 0

    def deserialize(self, f):
        self.locator = CBlockLocator()
        self.locator.deserialize(f)
        self.hashstop = deser_uint256(f)

    def serialize(self):
        r = b""
        r += self.locator.serialize()
        r += ser_uint256(self.hashstop)
        return r

    def __repr__(self):
        return "msg_getblocks(locator=%s hashstop=%064x)" % (repr(self.locator), self.hashstop)


class msg_getheaders:
    __slots__ = ("locator", "hashstop")
    command = b"getheaders"

    def __init__(self):
        self.locator = CBlockLocator()
        self.hashstop = 0

    def deserialize(self, f):
        self.locator = CBlockLocator()
        self.locator.deserialize(f)
        self.hashstop = deser_uint256(f)

    def serialize(self):
        r = b""
        r += self.locator.serialize()
        r += ser_uint256(self.hashstop)
        return r

    def __repr__(self):
        return "msg_getheaders(locator=%s hashstop=%064x)" % (repr(self.locator), self.hashstop)


class msg_headers:
    __slots__ = ("headers",)
    command = b"headers"

    def __init__(self, headers=None):
        self.headers = headers if headers is not None else []

    def deserialize(self, f):
        # The daemon sends a vector of bare CBlockHeader (src/main.cpp builds
        # vector<CBlockHeader>; CBlockHeader::SerializationOp writes header fields
        # only — no vtx count, no vchBlockSig). Reading full CBlocks here would
        # over-read into the next entry.
        self.headers = deser_vector(f, CBlockHeader)

    def serialize(self):
        # Emit header-only entries even if a CBlock was stored (ser_vector calls
        # each element's .serialize()).
        return ser_vector([CBlockHeader(h) for h in self.headers])

    def __repr__(self):
        return "msg_headers(headers=%s)" % repr(self.headers)


class msg_tx:
    __slots__ = ("tx",)
    command = b"tx"

    def __init__(self, tx=None):
        self.tx = tx if tx is not None else CTransaction()

    def deserialize(self, f):
        self.tx = CTransaction()
        self.tx.deserialize(f)

    def serialize(self):
        return self.tx.serialize()

    def __repr__(self):
        return "msg_tx(tx=%s)" % (repr(self.tx))


class msg_block:
    __slots__ = ("block",)
    command = b"block"

    def __init__(self, block=None):
        self.block = block if block is not None else CBlock()

    def deserialize(self, f):
        self.block = CBlock()
        self.block.deserialize(f)

    def serialize(self):
        return self.block.serialize()

    def __repr__(self):
        return "msg_block(block=%s)" % (repr(self.block))


class msg_getaddr:
    __slots__ = ()
    command = b"getaddr"

    def deserialize(self, f):
        pass

    def serialize(self):
        return b""

    def __repr__(self):
        return "msg_getaddr()"


class msg_mempool:
    __slots__ = ()
    command = b"mempool"

    def deserialize(self, f):
        pass

    def serialize(self):
        return b""

    def __repr__(self):
        return "msg_mempool()"


class msg_ping:
    __slots__ = ("nonce",)
    command = b"ping"

    def __init__(self, nonce=0):
        self.nonce = nonce

    def deserialize(self, f):
        self.nonce = struct.unpack("<Q", f.read(8))[0]

    def serialize(self):
        return struct.pack("<Q", self.nonce)

    def __repr__(self):
        return "msg_ping(nonce=%08x)" % self.nonce


class msg_pong:
    __slots__ = ("nonce",)
    command = b"pong"

    def __init__(self, nonce=0):
        self.nonce = nonce

    def deserialize(self, f):
        self.nonce = struct.unpack("<Q", f.read(8))[0]

    def serialize(self):
        return struct.pack("<Q", self.nonce)

    def __repr__(self):
        return "msg_pong(nonce=%08x)" % self.nonce


class msg_notfound:
    __slots__ = ("vec",)
    command = b"notfound"

    def __init__(self, vec=None):
        self.vec = vec or []

    def deserialize(self, f):
        self.vec = deser_vector(f, CInv)

    def serialize(self):
        return ser_vector(self.vec)

    def __repr__(self):
        return "msg_notfound(vec=%s)" % (repr(self.vec))


# Wire command -> message class for messages we parse. Includes Gridcoin's
# legacy aliases so an incoming "aries"/"gridaddr"/"encrypt" routes correctly.
MESSAGEMAP = {
    b"aries": msg_version,
    b"version": msg_version,
    b"verack": msg_verack,
    b"addr": msg_addr,
    b"gridaddr": msg_addr,
    b"inv": msg_inv,
    b"getdata": msg_getdata,
    b"getblocks": msg_getblocks,
    b"getheaders": msg_getheaders,
    b"headers": msg_headers,
    b"tx": msg_tx,
    b"block": msg_block,
    b"encrypt": msg_block,
    b"getaddr": msg_getaddr,
    b"mempool": msg_mempool,
    b"ping": msg_ping,
    b"pong": msg_pong,
    b"notfound": msg_notfound,
}
