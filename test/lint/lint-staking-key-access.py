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

ElevationGrant is restricted the same way. Its maker is CapabilityScope, so:

  * class CapabilityScope is defined only in src/wallet/capability.h. Until
    that file exists any definition fails, because one anywhere else would
    become the friend and could make grants.
  * The type ElevationGrant is named only in src/keystore.h, src/keystore.cpp,
    src/wallet/capability.h, src/wallet/capability.cpp and under src/test/.
    Elsewhere only its static members (ElevationGrant::IsActiveFor) may be
    used, so a grant cannot be constructed or held anywhere else.

Comments and string literals are ignored, so prose may name the factories.

If you are here because the lint failed: a staking path that needs keys under
StakingOnly gets its token from its own factory and passes it down; nothing else
should make one. Code that is not a staking path uses GetKey(), which works
under a Full unlock or inside a capability.
"""

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

# Subtrees that are not ours.
EXCLUDE_RE = re.compile(
    r"^src/(crypto/ctaes|leveldb|secp256k1|univalue|bdb53|ipc/libmultiprocess|minisketch|crc32c)/")

# Comments and string and character literals, matched in one left-to-right pass
# so that whichever starts first wins: a quote inside a comment, or a // inside
# a string, is then handled correctly.
NONCODE_RE = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:[^\"\\\n]|\\.)*\"|'(?:[^'\\\n]|\\.)*'", re.DOTALL)

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


def class_body(text, name):
    """Return (start, body) of `class name { ... }`, or None."""
    m = re.search(r"\bclass\s+{}\b[^{{;]*\{{".format(re.escape(name)), text)
    if not m:
        return None
    depth = 1
    i = m.end()
    while i < len(text) and depth:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    if depth:
        return None
    return m.end(), text[m.end():i - 1]


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


def source_files():
    out = subprocess.check_output(
        ["git", "ls-files", "--", "src/*.cpp", "src/*.h", "src/*.mm"], text=True)
    return [f for f in out.splitlines() if f and not EXCLUDE_RE.match(f)]


def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
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


def check_file(path, errors):
    code = strip_noncode(read(path))
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


def main():
    top = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()
    os.chdir(top)

    errors = []
    check_friend_lists(strip_noncode(read(KEYSTORE_HEADER)), errors)
    for path in source_files():
        check_file(path, errors)

    for e in errors:
        print("lint-staking-key-access: {}".format(e), file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
