#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Staking key access lint.

CCryptoKeyStore::GetKeyForStaking() decrypts a key while the wallet is unlocked
for staking only, where the general GetKey() refuses. It demands a
StakingKeyAccess, and what restricts that token is who can make one: the class
has a private constructor and names its makers as friends in src/keystore.h.
The compiler enforces the friend list, but not where the friends are called, so
this lint does:

  * Each factory is used only in the file of the staking path it serves
    (FACTORIES below). Any mention elsewhere -- a call, a redeclaration that
    would let another file call it, or taking its address -- fails. The friend
    declarations in src/keystore.h are the one exception.
  * The test peers, which make the same tokens for unit tests, are used only
    under src/test/ (and named in the friend declarations).
  * The friend lists of StakingKeyAccess and ElevationGrant are exactly the
    expected ones, so a new maker cannot be added without updating this lint,
    and none of those friends is defined inside the class, where it would be a
    maker no other check sees.

Who makes a token is half of it; who uses one is the other. Outside src/test/
(CONSUMERS below):

  * GetKeyForStaking() is called only in the files of the staking paths and of
    the signing provider that wraps it.
  * StakingSigningProvider, which signs with GetKeyForStaking(), is named only
    in its own files and in the two paths that sign with it: the miner's block
    signing and the automated beacon renewal's transaction.
  * The type StakingKeyAccess is named only in the files that hold or pass a
    token. A non-null renewal_access argument needs a token to point at, so
    those are also the only files that can pass one.
  * TrySignClaim(), which signs a claim with the miner's token, is called only
    by the miner.

ElevationGrant is restricted the same way. Its maker is CapabilityScope, so:

  * class CapabilityScope is defined only in src/wallet/capability.h. Until
    that file exists any definition fails, because one anywhere else would
    become the friend and could make grants.
  * The type ElevationGrant is named only in src/keystore.h, src/keystore.cpp,
    src/wallet/capability.h, src/wallet/capability.cpp and under src/test/.
    Elsewhere only its static members (ElevationGrant::IsActiveFor) may be
    used, so a grant cannot be constructed or held anywhere else.

The wallet keeps one IPC capability, in its IPC slot, for the connected
client's operations. Outside src/test/ (IPC_SLOT_FILES below):

  * IpcCapability(), BeginIpcElevation() and EndIpcElevation() are called
    only in the IPC interface implementations (src/wallet/interfaces.cpp,
    src/gridcoin/interfaces.cpp, src/node/interfaces.cpp) and in the wallet's
    own files, which declare and define them. The daemon's attach-slot hook
    (src/gridcoinresearchd.cpp), which ends the slot as the attached GUI's
    connection is torn down, makes exactly one call, EndIpcElevation({}), and
    no other. Code that serves another caller,
    an RPC request or an internal job, must not run under the IPC client's
    capability.

Every path to the wallet's private keys goes through GetKey(),
DecryptSecretWithMasterKey(), GetSeedPhraseBlob() or AddKeyPubKey(). The unit
suite key_entry_points_tests drives each entry point that reaches them, from
the table src/test/data/key_entry_points.json. The table's "sources" object
lists every file outside src/test/ that calls one of the four directly, with
the number of calls, so:

  * a direct call in a file the table does not list fails, and so does a
    listed file whose count no longer matches. A new direct call therefore
    forces an update of the table, which is the prompt to add the entry
    point's row. Calls through the wrappers -- SignSignature() and
    ProduceSignature(), CreateTransaction(), AddKey(), GenerateNewKey() and
    TopUpKeyPool(), SignPSGTInput() -- are not counted, so an entry point that
    reaches the keys only through one of them changes no count; the inventory
    in key_entry_points_tests is what catches that.
  * every IPC method the table names is declared on its interface in
    src/ipc/capnp/.

Comments and string literals are ignored, so prose may name any of these.
`--self-test` runs every rule against a sample that must fail it.

If you are here because the lint failed: a staking path that needs keys under
StakingOnly gets its token from its own factory and passes it down; nothing else
should make or hold one. Code that is not a staking path uses GetKey(), which
works under a Full unlock or inside a capability. A new direct caller of the
four key functions is a new path to the keys: add a row to
key_entry_points.json for each entry point that reaches it, run
key_entry_points_tests, then update the file's count under "sources".
"""

import json
import os
import re
import subprocess
import sys

KEYSTORE_HEADER = "src/keystore.h"

# Factory -> the one file that may define and call it.
FACTORIES = {
    "MinerStakingKeyAccess": "src/miner.cpp",
    "RenewalStakingKeyAccess": "src/gridcoin/researcher.cpp",
    "ScraperStakingKeyAccess": "src/gridcoin/scraper/scraper.cpp",
}

# The one file that may define class CapabilityScope, the maker of grants.
CAPABILITY_HEADER = "src/wallet/capability.h"

# The files that may name the type ElevationGrant other than to reach its
# static members. src/test/ is allowed as well.
GRANT_FILES = (
    "src/keystore.h",
    "src/keystore.cpp",
    CAPABILITY_HEADER,
    "src/wallet/capability.cpp",
)

# The files that may call the IPC capability slot's accessors, each with the
# calls it may make. None: any of them, any number of times (the IPC interface
# implementations, and the wallet's own files, which declare and define them).
# A list: exactly these calls, as (accessor, argument text), in this order; the
# daemon's attach-slot hook ends the slot once, with no connection check, as the
# connection is torn down. src/test/ is allowed as well.
IPC_SLOT_FILES = {
    "src/wallet/interfaces.cpp": None,
    "src/gridcoin/interfaces.cpp": None,
    "src/node/interfaces.cpp": None,
    "src/wallet/wallet.h": None,
    "src/wallet/wallet.cpp": None,
    "src/gridcoinresearchd.cpp": [("EndIpcElevation", "{}")],
}

IPC_SLOT_RE = re.compile(r"\b(IpcCapability|BeginIpcElevation|EndIpcElevation)\s*\(")

# Test-only makers, confined to the unit tests.
TEST_PEERS = ("StakingKeyAccessTestPeer", "ElevationGrantTestPeer")
TEST_DIR = "src/test/"

# The complete friend lists, as declared in src/keystore.h.
EXPECTED_FRIENDS = {
    "StakingKeyAccess": {
        "StakingKeyAccess MinerStakingKeyAccess()",
        "StakingKeyAccess RenewalStakingKeyAccess()",
        "StakingKeyAccess ScraperStakingKeyAccess()",
        "struct StakingKeyAccessTestPeer",
    },
    "ElevationGrant": {
        "class CapabilityScope",
        "struct ElevationGrantTestPeer",
    },
}

# The users of a token: the name, how a use of it is found, the files that may
# use it, and how the use reads in an error. src/test/ is allowed as well.
CONSUMERS = {
    "GetKeyForStaking": (
        re.compile(r"\bGetKeyForStaking\s*\("),
        (
            "src/keystore.h",
            "src/keystore.cpp",
            "src/miner.cpp",
            "src/gridcoin/researcher.cpp",
            "src/gridcoin/scraper/scraper.cpp",
            "src/wallet/staking_signer.cpp",
        ),
        "called",
    ),
    "StakingSigningProvider": (
        re.compile(r"\bStakingSigningProvider\b"),
        (
            "src/wallet/staking_signer.h",
            "src/wallet/staking_signer.cpp",
            "src/miner.cpp",
            "src/wallet/wallet.cpp",
        ),
        "named",
    ),
    "StakingKeyAccess": (
        re.compile(r"\bStakingKeyAccess\b"),
        (
            "src/keystore.h",
            "src/keystore.cpp",
            "src/miner.cpp",
            "src/gridcoin/researcher.h",
            "src/gridcoin/researcher.cpp",
            "src/gridcoin/scraper/scraper.h",
            "src/gridcoin/scraper/scraper.cpp",
            "src/gridcoin/contract/message.h",
            "src/gridcoin/contract/message.cpp",
            "src/wallet/staking_signer.h",
            "src/wallet/staking_signer.cpp",
            "src/wallet/wallet.h",
            "src/wallet/wallet.cpp",
        ),
        "named (to hold a token, or to pass one as a non-null renewal_access argument)",
    ),
    "TrySignClaim": (
        re.compile(r"\bTrySignClaim\s*\("),
        (
            "src/miner.h",
            "src/miner.cpp",
        ),
        "called",
    ),
}

# The table the unit suite key_entry_points_tests drives.
TABLE = "src/test/data/key_entry_points.json"

# A call or declaration of one of the four routes to key material. It needs an
# argument, so the argument-less GetKey() of CService and the address manager
# is not counted.
KEY_FUNCTION_RE = re.compile(
    r"\b(GetKey|DecryptSecretWithMasterKey|GetSeedPhraseBlob|AddKeyPubKey)\s*\(\s*[^)\s]")

CAPNP_DIR = "src/ipc/capnp/"

# Subtrees that are not ours.
EXCLUDE_RE = re.compile(
    r"^src/(crypto/ctaes|leveldb|secp256k1|univalue|bdb53|ipc/libmultiprocess|minisketch|crc32c)/")

# Comments and string and character literals, matched in one left-to-right pass
# so that whichever starts first wins: a quote inside a comment, or a // inside
# a string, is then handled correctly.
NONCODE_RE = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:[^\"\\\n]|\\.)*\"|'(?:[^'\\\n]|\\.)*'", re.DOTALL)

# Cap'n Proto comments run from # to the end of the line.
CAPNP_COMMENT_RE = re.compile(r"#[^\n]*")

FACTORY_RE = re.compile(r"\b({})\b".format("|".join(sorted(FACTORIES))))
PEER_RE = re.compile(r"\b({})\b".format("|".join(TEST_PEERS)))
# A friend declaration ends at ';', a friend defined in the class at '{'.
FRIEND_RE = re.compile(r"\bfriend\s+([^;{]*?)\s*([;{])")

# A definition of class CapabilityScope: the name followed by a base clause or
# the class body, not by ';' as in a declaration or a friend declaration.
CAPABILITY_SCOPE_DEF_RE = re.compile(
    r"\b(?:class|struct)\s+(?:\[\[.*?\]\]\s*)*CapabilityScope\b\s*(?:final\b\s*)?[:{]")

# The type ElevationGrant named for anything but its static members.
GRANT_RE = re.compile(r"\bElevationGrant\b(?!\s*::)")


def strip_noncode(text):
    """Blank comments and literals, preserving newlines for line numbers."""
    def blank(m):
        return "".join(ch if ch == "\n" else " " for ch in m.group(0))
    return NONCODE_RE.sub(blank, text)


def normalize(decl):
    """Collapse whitespace so a declaration compares by its tokens."""
    decl = re.sub(r"\s+", " ", decl).strip()
    return re.sub(r"\s*\(\s*\)", "()", decl)


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


def braced(text, start):
    """The text from start up to the brace closing the one just before start, or None."""
    depth = 1
    i = start
    while i < len(text) and depth:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    if depth:
        return None
    return text[start:i - 1]


def call_arguments(text, open_paren):
    """The text between the parenthesis at open_paren and the one closing it,
    with whitespace collapsed, or None when it is not closed."""
    depth = 0
    for i in range(open_paren, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return re.sub(r"\s+", " ", text[open_paren + 1:i]).strip()
    return None


def describe_slot_calls(calls):
    return ", ".join("{}({})".format(name, args) for name, args in calls) or "none"


def class_body(text, name):
    """Return (start, body) of `class name { ... }`, or None."""
    m = re.search(r"\bclass\s+{}\b[^{{;]*\{{".format(re.escape(name)), text)
    if not m:
        return None
    body = braced(text, m.end())
    if body is None:
        return None
    return m.end(), body


def friend_spans(code):
    """Spans of the friend declarations of the classes this lint guards."""
    spans = []
    for name in EXPECTED_FRIENDS:
        found = class_body(code, name)
        if found is None:
            continue
        start, body = found
        for m in FRIEND_RE.finditer(body):
            spans.append((start + m.start(), start + m.end()))
    return spans


def inside(pos, spans):
    return any(lo <= pos < hi for lo, hi in spans)


def git_files(*patterns):
    out = subprocess.check_output(["git", "ls-files", "--"] + list(patterns), text=True)
    return [f for f in out.splitlines() if f]


def source_files():
    return [f for f in git_files("src/*.cpp", "src/*.h", "src/*.mm") if not EXCLUDE_RE.match(f)]


def read(path):
    with open(path, "r", encoding="utf8", errors="replace") as f:
        return f.read()


def check_friend_lists(code, errors):
    for name, expected in EXPECTED_FRIENDS.items():
        found = class_body(code, name)
        if found is None:
            errors.append("{}: class {} not found".format(KEYSTORE_HEADER, name))
            continue
        _, body = found
        actual = set()
        for m in FRIEND_RE.finditer(body):
            decl = normalize(m.group(1))
            actual.add(decl)
            if m.group(2) == "{":
                errors.append("{}: class {} defines friend '{}' inside the class; its friends "
                              "must be declarations only".format(KEYSTORE_HEADER, name, decl))
        for extra in sorted(actual - expected):
            errors.append("{}: class {} has an unexpected friend '{}'; a new maker must be "
                          "added to this lint deliberately".format(KEYSTORE_HEADER, name, extra))
        for missing in sorted(expected - actual):
            errors.append("{}: class {} no longer declares friend '{}'; update this lint "
                          "to match".format(KEYSTORE_HEADER, name, missing))


def check_code(path, code, errors, key_calls):
    """Check one file's code, its comments and literals already blanked.

    The calls of the four key functions it makes outside src/test/ are recorded
    in key_calls[path], as (line, name), for check_sources().
    """
    allowed = friend_spans(code) if path == KEYSTORE_HEADER else []

    for m in FACTORY_RE.finditer(code):
        name = m.group(1)
        if path == FACTORIES[name] or inside(m.start(), allowed):
            continue
        errors.append("{}:{}: {} is used outside {}".format(
            path, line_of(code, m.start()), name, FACTORIES[name]))

    if path != CAPABILITY_HEADER:
        for m in CAPABILITY_SCOPE_DEF_RE.finditer(code):
            errors.append("{}:{}: class CapabilityScope is defined outside {}".format(
                path, line_of(code, m.start()), CAPABILITY_HEADER))

    if path.startswith(TEST_DIR):
        return

    if path not in IPC_SLOT_FILES:
        for m in IPC_SLOT_RE.finditer(code):
            errors.append("{}:{}: {}() is called outside the IPC interface files ({}); the IPC slot's "
                          "capability is the connected client's, for the interface methods that serve it"
                          .format(path, line_of(code, m.start()), m.group(1),
                                  ", ".join(p for p, calls in IPC_SLOT_FILES.items() if calls is None)))
    elif IPC_SLOT_FILES[path] is not None:
        found = [(m.group(1), call_arguments(code, m.end() - 1)) for m in IPC_SLOT_RE.finditer(code)]
        if found != list(IPC_SLOT_FILES[path]):
            errors.append("{}: the IPC slot is called as {}; this file may make only {}"
                          .format(path, describe_slot_calls(found), describe_slot_calls(IPC_SLOT_FILES[path])))

    if path not in GRANT_FILES:
        for m in GRANT_RE.finditer(code):
            errors.append("{}:{}: ElevationGrant is used outside {} and {}; elsewhere only its "
                          "static members may be".format(path, line_of(code, m.start()),
                                                         ", ".join(GRANT_FILES), TEST_DIR))

    for m in PEER_RE.finditer(code):
        if inside(m.start(), allowed):
            continue
        errors.append("{}:{}: {} is used outside {}".format(
            path, line_of(code, m.start()), m.group(1), TEST_DIR))

    for name, (pattern, files, how) in CONSUMERS.items():
        if path in files:
            continue
        for m in pattern.finditer(code):
            errors.append("{}:{}: {} is {} outside the staking paths ({} and {})".format(
                path, line_of(code, m.start()), name, how, ", ".join(files), TEST_DIR))

    for m in KEY_FUNCTION_RE.finditer(code):
        key_calls.setdefault(path, []).append((line_of(code, m.start()), m.group(1)))


def check_sources(key_calls, listed, errors):
    """Compare the key-function calls found with the table's "sources"."""
    for path in sorted(key_calls):
        calls = key_calls[path]
        if path not in listed:
            for line, name in calls:
                errors.append("{}:{}: {}() reaches the wallet's keys in a file {} does not list "
                              "under \"sources\"; add a row for the entry point it serves, then "
                              "list the file".format(path, line, name, TABLE))
        elif listed[path] != len(calls):
            errors.append("{}: {} calls of the key functions, but {} lists {}; a new call is a "
                          "new path to the keys: add its row, then set the count (a removed call "
                          "needs only the count)".format(path, len(calls), TABLE, listed[path]))

    for path in sorted(listed):
        if path not in key_calls:
            errors.append("{}: listed under \"sources\" in {}, but it calls none of the key "
                          "functions; remove it".format(path, TABLE))


def capnp_methods(texts):
    """Interface name -> the names of its methods, from capnp file texts."""
    methods = {}
    for text in texts:
        code = CAPNP_COMMENT_RE.sub("", text)
        for m in re.finditer(r"\binterface\s+(\w+)[^{]*\{", code):
            body = braced(code, m.end())
            if body is None:
                continue
            methods.setdefault(m.group(1), set()).update(
                re.findall(r"^\s*(\w+)\s*@\d+", body, re.MULTILINE))
    return methods


def check_ipc_rows(table, methods, errors):
    for section in ("ipc", "ipc_pool_draw", "ipc_not_rows"):
        for row in table.get(section, []):
            interface = row.get("interface")
            method = row.get("method")
            if interface not in methods:
                errors.append("{}: \"{}\" names interface {}, which no file in {} declares".format(
                    TABLE, section, interface, CAPNP_DIR))
            elif method not in methods[interface]:
                errors.append("{}: \"{}\" names {}.{}, which the interface does not declare".format(
                    TABLE, section, interface, method))


def run(files, header, table, capnp_texts):
    """Every check, over the source texts {path: text}, the text of
    src/keystore.h, the table and the capnp file texts. Returns the errors."""
    errors = []
    check_friend_lists(strip_noncode(header), errors)

    key_calls = {}
    for path in sorted(files):
        check_code(path, strip_noncode(files[path]), errors, key_calls)

    listed = table.get("sources", {})
    if not isinstance(listed, dict):
        errors.append("{}: \"sources\" must be an object of file -> number of calls".format(TABLE))
        listed = {}
    check_sources(key_calls, listed, errors)
    check_ipc_rows(table, capnp_methods(capnp_texts), errors)

    return errors


# The self-test. Each failing sample breaks one rule of a clean tree.

SAMPLE_HEADER = """
class ElevationGrant
{
    friend class CapabilityScope;
    friend struct ElevationGrantTestPeer;
};
class StakingKeyAccess
{
    friend StakingKeyAccess MinerStakingKeyAccess();
    friend StakingKeyAccess RenewalStakingKeyAccess();
    friend StakingKeyAccess ScraperStakingKeyAccess();
    friend struct StakingKeyAccessTestPeer;
};
"""

SAMPLE_CAPNP = """
interface Wallet $Proxy.wrap("interfaces::Wallet") {
    signMessage @22 (context :Proxy.Context, address :Text) -> (result :Int32);
    # removedMethod @23 () -> ();
}
"""

SAMPLE_TABLE = {
    "ipc": [{"interface": "Wallet", "method": "signMessage"}],
    "sources": {"src/wallet/rpcwallet.cpp": 1},
}

# Code every rule accepts: prose and literals naming everything, the address
# manager's argument-less GetKey(), the miner's own factory and claim signing,
# and the one key call the sample table lists.
CLEAN_FILES = {
    "src/wallet/rpcwallet.cpp": (
        "// MinerStakingKeyAccess(), GetKeyForStaking(id, key, access), TrySignClaim(a)\n"
        "/* StakingSigningProvider p(store, access); ElevationGrant g; */\n"
        "const char* s = \"GetKey(id, key) StakingKeyAccess\";\n"
        "bool f() { CKey key; return pwalletMain->GetKey(id, key); }\n"),
    "src/addrman.cpp": "uint64_t h = (CHashWriter(SER_GETHASH, 0) << GetKey()).GetCheapHash();\n",
    "src/miner.cpp": (
        "StakingKeyAccess MinerStakingKeyAccess() { return StakingKeyAccess(); }\n"
        "bool g() { return TrySignClaim(pwallet, claim, block); }\n"),
    "src/wallet/interfaces.cpp": (
        "bool b() { return w->BeginIpcElevation(p, k, m_connection_closed); }\n"
        "void s() { CapabilityScope scope(*w, w->IpcCapability(m_connection_closed), kind); }\n"
        "void e() { w->EndIpcElevation(m_connection_closed); }\n"),
    "src/gridcoinresearchd.cpp": (
        "auto hook = [] { if (pwalletMain) pwalletMain->EndIpcElevation({}); };\n"),
}

# (the rule it breaks, the error that rule must report, files added to the
# clean ones, and the header, table and capnp text when they differ from the
# samples above)
FAILING_SAMPLES = [
    ("a factory used outside its file",
     "src/rpc/blockchain.cpp:1: MinerStakingKeyAccess is used outside src/miner.cpp",
     {"src/rpc/blockchain.cpp": "void f() { auto a = MinerStakingKeyAccess(); }\n"}, None, None, None),
    ("a test peer used outside src/test/",
     "src/init.cpp:1: StakingKeyAccessTestPeer is used outside src/test/",
     {"src/init.cpp": "void f() { StakingKeyAccessTestPeer::Make(); }\n"}, None, None, None),
    ("CapabilityScope defined outside its header",
     "src/wallet/wallet.cpp:1: class CapabilityScope is defined outside src/wallet/capability.h",
     {"src/wallet/wallet.cpp": "class CapabilityScope : public Base {};\n"}, None, None, None),
    ("ElevationGrant named outside its files",
     "src/wallet/wallet.cpp:1: ElevationGrant is used outside",
     {"src/wallet/wallet.cpp": "std::unique_ptr<ElevationGrant> grant;\n"}, None, None, None),
    ("GetKeyForStaking called outside its consumers",
     "src/rpc/blockchain.cpp:1: GetKeyForStaking is called outside the staking paths",
     {"src/rpc/blockchain.cpp": "bool f() { return w.GetKeyForStaking(id, key, *access); }\n"},
     None, None, None),
    ("StakingSigningProvider made outside its consumers",
     "src/rpc/rawtransaction.cpp:1: StakingSigningProvider is named outside the staking paths",
     {"src/rpc/rawtransaction.cpp": "void f() { const StakingSigningProvider p(*w, a); }\n"},
     None, None, None),
    ("a non-null renewal_access passed outside the staking paths",
     "src/gridcoin/interfaces.cpp:1: StakingKeyAccess is named (to hold a token, or to pass one as a "
     "non-null renewal_access argument) outside the staking paths",
     {"src/gridcoin/interfaces.cpp":
      "void f(const StakingKeyAccess* access) { r->AdvertiseBeacon(false, access); }\n"},
     None, None, None),
    ("TrySignClaim called outside the miner",
     "src/gridcoin/mrc.cpp:1: TrySignClaim is called outside the staking paths",
     {"src/gridcoin/mrc.cpp": "bool f() { return TrySignClaim(w, claim, block); }\n"},
     None, None, None),
    ("a key function called in a file the table does not list",
     "src/rpc/misc.cpp:1: GetSeedPhraseBlob() reaches the wallet's keys in a file",
     {"src/rpc/misc.cpp": "bool f() { return pwalletMain->GetSeedPhraseBlob(blob); }\n"},
     None, None, None),
    ("one more key function call in a listed file",
     "src/wallet/rpcwallet.cpp: 2 calls of the key functions, but",
     {"src/wallet/rpcwallet.cpp": CLEAN_FILES["src/wallet/rpcwallet.cpp"]
      + "bool g() { return pwalletMain->AddKeyPubKey(key, pubkey); }\n"},
     None, None, None),
    ("a listed file that calls no key function",
     "src/net.cpp: listed under \"sources\"",
     {}, None, dict(SAMPLE_TABLE, sources={"src/wallet/rpcwallet.cpp": 1, "src/net.cpp": 1}), None),
    ("an unexpected friend of StakingKeyAccess",
     "class StakingKeyAccess has an unexpected friend 'void UnlistedMaker()'",
     {}, SAMPLE_HEADER.replace("friend struct StakingKeyAccessTestPeer;",
                               "friend struct StakingKeyAccessTestPeer;\n    friend void UnlistedMaker();"),
     None, None),
    ("a friend defined inside the class",
     "class ElevationGrant defines friend 'class CapabilityScope' inside the class",
     {}, SAMPLE_HEADER.replace("friend class CapabilityScope;", "friend class CapabilityScope { };"),
     None, None),
    ("an IPC row naming a method its interface does not declare",
     "names Wallet.removedMethod, which the interface does not declare",
     {}, None, dict(SAMPLE_TABLE, ipc=[{"interface": "Wallet", "method": "removedMethod"}]), None),
    ("the IPC capability slot read outside the IPC interfaces",
     "src/wallet/rpcwallet.cpp:5: IpcCapability() is called outside the IPC interface files",
     {"src/wallet/rpcwallet.cpp": CLEAN_FILES["src/wallet/rpcwallet.cpp"]
      + "void g() { CapabilityScope s(*pwalletMain, pwalletMain->IpcCapability({}), kind); }\n"},
     None, None, None),
    ("the IPC capability slot begun outside the IPC interfaces",
     "src/rpc/misc.cpp:1: BeginIpcElevation() is called outside the IPC interface files",
     {"src/rpc/misc.cpp": "bool f() { return pwalletMain->BeginIpcElevation(p, k, {}); }\n"},
     None, None, None),
    ("the daemon's slot hook reading the slot",
     "src/gridcoinresearchd.cpp: the IPC slot is called as EndIpcElevation({}), IpcCapability({}); "
     "this file may make only EndIpcElevation({})",
     {"src/gridcoinresearchd.cpp": CLEAN_FILES["src/gridcoinresearchd.cpp"]
      + "auto id = pwalletMain->IpcCapability({});\n"},
     None, None, None),
    ("the daemon's slot hook passing a connection check",
     "src/gridcoinresearchd.cpp: the IPC slot is called as EndIpcElevation(closed); "
     "this file may make only EndIpcElevation({})",
     {"src/gridcoinresearchd.cpp": "auto hook = [] { pwalletMain->EndIpcElevation(closed); };\n"},
     None, None, None),
    ("an IPC row naming an interface no capnp file declares",
     "names interface Vault, which no file in src/ipc/capnp/ declares",
     {}, None, dict(SAMPLE_TABLE, ipc=[{"interface": "Vault", "method": "signMessage"}]), None),
]


def self_test():
    failures = []

    clean = run(dict(CLEAN_FILES), SAMPLE_HEADER, SAMPLE_TABLE, [SAMPLE_CAPNP])
    if clean:
        failures.append("the clean sample fails: {}".format("; ".join(clean)))

    for what, expected, files, header, table, capnp in FAILING_SAMPLES:
        merged = dict(CLEAN_FILES)
        merged.update(files)
        errors = run(merged,
                     SAMPLE_HEADER if header is None else header,
                     SAMPLE_TABLE if table is None else table,
                     [SAMPLE_CAPNP if capnp is None else capnp])
        if not any(expected in e for e in errors):
            failures.append("no error '{}' for {}; got: {}".format(
                expected, what, "; ".join(errors) if errors else "none"))

    for failure in failures:
        print("lint-staking-key-access self-test: {}".format(failure), file=sys.stderr)
    if not failures:
        print("lint-staking-key-access self-test: each of {} samples fails with its rule's error, "
              "and the clean sample passes".format(len(FAILING_SAMPLES)))
    return 1 if failures else 0


def main():
    top = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()
    os.chdir(top)

    if sys.argv[1:] == ["--self-test"]:
        return self_test()
    if sys.argv[1:]:
        print("usage: {} [--self-test]".format(sys.argv[0]), file=sys.stderr)
        return 2

    with open(TABLE, "r", encoding="utf8") as f:
        table = json.load(f)

    files = {path: read(path) for path in source_files()}
    errors = run(files, files[KEYSTORE_HEADER], table,
                 [read(path) for path in git_files(CAPNP_DIR + "*.capnp")])

    for e in errors:
        print("lint-staking-key-access: {}".format(e), file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
