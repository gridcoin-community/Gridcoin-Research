#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# RPC heritage ledger enforcement (issue #3069).
#
# The heritage classification lives in a COLUMN on each src/rpc/server.cpp vRPCCommands[] row:
#   { "name", &impl, cat_xxx, &help_helpman, heritage_<bucket>, "<fp>" }
# The CRPCCommand constructor makes every field mandatory, so a command cannot be registered
# without a bucket (classify-at-birth). This lint reads that column -- the same table the
# dispatcher reads -- and over every registered RPC checks:
#   1. PRESENCE (all buckets): the row's heritage_<bucket> parses to a known bucket.
#   2. SURFACE-FINGERPRINT DRIFT (pure-upstream, mixed & removed-upstream): the recomputed
#      surface fingerprint = sha256("ARGS:" + args(in RPCHelpMan declaration order) + "|KEYS:" +
#      sorted(result pushKV keys, gathered by recursive descent through called result-builder
#      helpers -- cycle-safe, bounded by MAX_DESCENT))[:12] must equal the row's heritage_fp.
#      A mismatch means the input/output surface changed -> re-confirm the heritage bucket and
#      update the row + doc/rpc-heritage.md. Recursive descent captures NESTED schemas (e.g. a
#      tx RPC whose output transitively renders a Gridcoin contract payload), so the fingerprint
#      reflects the full observable surface, not just the top level. Output that isn't
#      literal-key-trackable uses heritage_fp "manual"; pure-gridcoin is NOT fingerprinted (fp "").
#   3. Several integrity assertions (see check_* below): every vRPCCommands row parses; a
#      fingerprinted RPC's captured surface is trackable; pure-gridcoin carries no fp;
#      doc/rpc-heritage.md agrees with the heritage column row-by-row.
#
# Resolution is deterministic: files are processed in sorted order and only function
# DEFINITIONS (not extern/`;`-terminated declarations) are matched.

import glob
import hashlib
import os
import re
import sys
from collections import deque

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = sorted(sum([glob.glob(f"{REPO}/src/rpc/**/*.cpp", recursive=True),
                  glob.glob(f"{REPO}/src/wallet/**/*.cpp", recursive=True),
                  glob.glob(f"{REPO}/src/gridcoin/**/*.cpp", recursive=True)], []))
TEXTS = [(f, open(f, encoding="utf-8", errors="replace").read()) for f in SRC]
PUSHKV_RE = re.compile(r'pushKV\(\s*"([^"]+)"')


def _block(text, start):
    """Substring from the first '{' at/after start through its matching '}'."""
    i = text.find('{', start)
    if i < 0:
        return ""
    depth = 0
    j = i
    while j < len(text):
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
        j += 1
    return text[i:]


_def_cache = {}


def def_pos(name, rets=("UniValue", "void", "bool")):
    """(file_text, index) of the DEFINITION of function `name`, or (None, None).
    Skips extern/`;`-terminated forward declarations (a definition has a `{` body
    before the next `;`). Deterministic via sorted TEXTS."""
    if name in _def_cache:
        return _def_cache[name]
    pat = re.compile(rf'\b(?:static\s+)?(?:{"|".join(rets)})\s+{re.escape(name)}\s*\(')
    for t in [x[1] for x in TEXTS]:
        for m in pat.finditer(t):
            tail = t[m.end():]
            semi = tail.find(';')
            brace = tail.find('{')
            if brace != -1 and (semi == -1 or brace < semi):
                _def_cache[name] = (t, m.start())
                return _def_cache[name]
    _def_cache[name] = (None, None)
    return _def_cache[name]


def def_body(name, rets=("UniValue", "void", "bool")):
    t, i = def_pos(name, rets)
    return _block(t, i) if t is not None else None


# Cap on result-builder recursion depth. check_result_builder_reachability() asserts that
# no builder reachable from an RPC sits beyond this depth (i.e. the cap never truncates a
# real surface); raise it only if that check ever fires.
MAX_DESCENT = 8

# Functions that build the JSON-RPC *envelope*, not an RPC *result*. They emit pushKV
# ("code"/"message", "result"/"error"/"id") and are reached from nearly every RPC via
# `throw JSONRPCError(...)`, so without this exclusion the error envelope would pollute the
# result surface of every throwing RPC (see issue #3069 review). They are NOT result-builders.
_EXCLUDE_BUILDERS = frozenset({"JSONRPCError", "JSONRPCReplyObj"})
_builders_cache = None


def builders():
    """name -> body for every result-builder defined here: a function returning
    UniValue/void/bool whose body emits a result key (pushKV) or appends to a result array
    (push_back). Definition-aware (skips extern/`;`-decls); first definition in sorted file
    order wins (deterministic). This is the node set of the result-builder call graph that
    surface() descends and the reachability check walks. Including push_back array builders
    lets the descent pass THROUGH array wrappers (e.g. a function that only
    push_back(InnerToJson(...))) to reach the nested object schema.

    Limitation: resolution is by unqualified name with first-definition-wins, so C++ overloads
    and member/operator definitions collapse to a single body. No fingerprinted RPC currently
    routes through an overloaded or member result-builder; if one ever does, the captured
    surface is whichever definition sorts first and the others' keys are silently dropped."""
    global _builders_cache
    if _builders_cache is not None:
        return _builders_cache
    out = {}
    pat = re.compile(r'\b(?:static\s+)?(?:UniValue|void|bool)\s+([A-Za-z_]\w*)\s*\(')
    for _, t in TEXTS:
        for m in pat.finditer(t):
            name = m.group(1)
            if name in out or name in _EXCLUDE_BUILDERS:
                continue
            tail = t[m.end():]
            semi = tail.find(';')
            brace = tail.find('{')
            if brace == -1 or (semi != -1 and semi < brace):
                continue  # extern/forward declaration, not a definition
            body = _block(t, m.start())
            if 'pushKV(' in body or 'push_back(' in body:
                out[name] = body
    _builders_cache = out
    return out


def _calls(body):
    """Function names invoked in `body`, EXCLUDING constructor-style UniValue declarations
    such as `UniValue projects(UniValue::VOBJ)` -- these look like a function call but are
    local-variable declarations. Following one would wrongly pull an unrelated same-named
    function's body into the descent (issue #3069 review: getblock over-captured the `projects`
    RPC's keys via `UniValue projects(UniValue::VOBJ)` inside SuperblockToJson)."""
    out = set()
    for m in re.finditer(r'\b([A-Za-z_]\w*)\s*\(', body):
        # Peek (without consuming) at what follows '(' so a nested call passed as an argument,
        # e.g. push_back(ContractToJson(contract)), is NOT swallowed -- both names are kept.
        if body[m.end():m.end() + 20].lstrip().startswith('UniValue::V'):
            continue  # `Name(UniValue::VOBJ/VARR/...)` is a declaration, not a call
        out.add(m.group(1))
    return out


def _reachable_bodies(impl):
    """Bodies reachable from `impl`'s definition through the result-builder call graph
    (impl included), cycle-safe via a per-path visited set and bounded by MAX_DESCENT.
    surface() unions pushKV keys over these. The traversal follows any called function that is
    itself a result-builder (pushKV or push_back), so a change anywhere in the reachable
    subtree shows up as drift."""
    B = builders()
    body = def_body(impl, ("UniValue",))
    if not body:
        return []
    out = []

    def walk(b, visited, depth):
        out.append(b)
        if depth >= MAX_DESCENT:
            return
        for fn in _calls(b):
            if fn in visited or fn not in B:
                continue
            walk(B[fn], visited | {fn}, depth + 1)

    walk(body, {impl}, 0)
    return out


# vRPCCommands[] row:
#   { "name", &impl, cat_xxx, &help_helpman, heritage_<bucket>, "<fp>" }
# The heritage classification + fingerprint baseline are read straight off this row
# (issue #3069) -- the lint reads the same table the dispatcher does; no C++-body or
# comment parsing for the classification. The CRPCCommand constructor makes all fields
# mandatory, so a new row cannot omit the bucket (classify-at-birth).
ROW_RE = re.compile(r'\{\s*"([a-z0-9]+)"\s*,\s*&(\w+)\s*,\s*\w+\s*,\s*&(\w+)\s*,'
                    r'\s*heritage_(\w+)\s*,\s*"([0-9a-f]{12}|manual|)"\s*\}')
ENUM_LABEL = {"pure_upstream": "pure-upstream", "mixed": "mixed",
              "removed_upstream": "removed-upstream", "pure_gridcoin": "pure-gridcoin"}


def rpc_table():
    """(table, users, n_candidate_rows, heritage): table maps name -> (impl, helpvar);
    heritage maps name -> (bucket_label, fp) read from the vRPCCommands[] row."""
    t = open(f"{REPO}/src/rpc/server.cpp", encoding="utf-8", errors="replace").read()
    seg = t[t.index("vRPCCommands[]"):t.index("\n};", t.index("vRPCCommands[]"))]
    out = {}
    users = {}
    heritage = {}
    for m in ROW_RE.finditer(seg):
        name, impl, hm, enum, fp = m.group(1), m.group(2), m.group(3), m.group(4), m.group(5)
        helpvar = hm[:-len("_helpman")] + "_help" if hm.endswith("_helpman") else hm
        out[name] = (impl, helpvar)
        users.setdefault(helpvar, []).append(name)
        heritage[name] = (ENUM_LABEL.get(enum), fp)
    # Count candidate rows name-AGNOSTICALLY (any "<name>", & ... opener), so a row ROW_RE
    # cannot parse -- e.g. an out-of-[a-z0-9] command name, or a malformed heritage column --
    # inflates n_candidates above len(TABLE) and trips check_table_complete() rather than
    # vanishing silently. (ROW_RE itself stays strict; the mismatch is the alarm.)
    n_candidates = len(re.findall(r'^\s*\{\s*"[^"]+"\s*,\s*&', seg, re.M))
    return out, users, n_candidates, heritage


TABLE, USERS, N_ROWS, HERITAGE = rpc_table()


def owner_of(helpvar):
    u = USERS[helpvar]
    for n in u:
        if f"{n}_help" == helpvar:
            return n
    for n in u:
        if TABLE[n][0] == helpvar[:-len("_help")]:
            return n
    return sorted(u)[0]


def surface(name):
    """Return (arg names in declaration order, sorted result pushKV keys).
    Result keys = pushKV in the impl body, unioned with the keys of every result-builder
    reachable from it by recursive descent (cycle-safe, bounded by MAX_DESCENT). This
    captures nested schemas -- e.g. a tx RPC whose output transitively renders a Gridcoin
    contract payload contributes the contract's keys to the fingerprint."""
    impl, helpvar = TABLE[name]
    args = []
    for _, t in TEXTS:
        m = re.search(rf'RPCHelpMan\s+{re.escape(helpvar)}\b', t)
        if m:
            args = re.findall(r'\{\s*"([^"]+)"\s*,\s*RPCArg::Type', _block(t, m.start()))
            break
    keys = set()
    for body in _reachable_bodies(impl):
        keys |= set(PUSHKV_RE.findall(body))
    return args, sorted(keys)


def _returns_dynamic_or_array(fn, seen):
    """True iff `fn` RETURNS a UniValue positional array (a var declared `UniValue x(VARR)`) or
    a dynamically-keyed object (a `UniValue x(VOBJ)` that receives pushKV but, since the caller
    already found no literal keys, only variable keys). Follows a single delegated builder call
    `return Foo(...)`. A scalar return (HexStr/EncodeBase64/x.GetHex()/literal) is NOT either.
    Cycle-safe via `seen`. This keys off what is RETURNED, not what a reachable helper happens
    to construct -- so a std::vector push_back or an unrelated array helper is not mistaken for
    the result (issue #3069 review: 5 scalar-returning RPCs were wrongly forced to fp=manual)."""
    if fn in seen:
        return False
    seen.add(fn)
    body = def_body(fn, ("UniValue",))
    if not body:
        return False
    for expr in re.findall(r'\breturn\s+([^;]+);', body):
        expr = expr.strip()
        if re.fullmatch(r'[A-Za-z_]\w*', expr):  # returns a bare local UniValue variable
            if re.search(rf'\bUniValue\s+{re.escape(expr)}\s*\(\s*UniValue::VARR', body):
                return True  # positional UniValue array
            if (re.search(rf'\bUniValue\s+{re.escape(expr)}\s*\(\s*UniValue::VOBJ', body)
                    and re.search(rf'\b{re.escape(expr)}\.pushKV\(', body)):
                return True  # dynamically-keyed object (literal keys already empty)
            continue
        call = re.match(r'([A-Za-z_]\w*)\s*\(', expr)  # delegated builder: return Foo(...)
        if call and call.group(1) != fn and call.group(1) in builders():
            if _returns_dynamic_or_array(call.group(1), seen):
                return True
    return False


def output_uncapturable(name):
    """True when the literal-key fingerprint cannot represent the RPC's RESULT, so the marker
    must use fp=manual (drift reviewed by hand). Determined from what the impl RETURNS -- a
    UniValue positional array (e.g. getrawmempool) or a dynamically-keyed object (e.g.
    `logging`'s pushKV(category, active)) -- following a single delegated builder call. A
    scalar/null return (e.g. createrawtransaction's HexStr, sendmany's txid) or a keyed object
    (non-empty literal keys) is capturable and uses a hash fp over the args + keys."""
    _, keys = surface(name)
    if keys:
        return False
    return _returns_dynamic_or_array(TABLE[name][0], set())


def fingerprint(name):
    args, keys = surface(name)
    payload = "ARGS:" + ",".join(args) + "|KEYS:" + ",".join(keys)
    return hashlib.sha256(payload.encode()).hexdigest()[:12]


# Buckets whose surface is fingerprint-tracked. removed-upstream is a fingerprinted sibling of
# mixed: upstream-derived RPCs that current upstream deleted (legacy-wallet sunset, alert system,
# the account family, signrawtransaction split, ...). They have no current backport target, but
# they are frozen Bitcoin-lineage forks -- the fingerprint pins the frozen surface so any change
# (including a deliberate pull up to the last valid pre-removal upstream version) is a controlled,
# re-confirmed event rather than silent drift. pure-gridcoin is NOT fingerprinted.
FINGERPRINTED = ("pure-upstream", "mixed", "removed-upstream")


def find_marker(name):
    """(bucket_label, fp) for this RPC, read from its vRPCCommands[] heritage column.
    fp is a 12-hex surface fingerprint, "manual" (output not literal-key-trackable, reviewed
    by hand), or "" for pure-gridcoin (not fingerprinted)."""
    return HERITAGE.get(name, (None, None))


def check_table_complete():
    """Every vRPCCommands command row must parse into TABLE (no silent drops)."""
    if len(TABLE) >= N_ROWS:
        return []
    return [f"vRPCCommands: {N_ROWS - len(TABLE)} command row(s) did not parse into the "
            f"heritage table (regex/shape mismatch) -> those RPCs are silently unenforced"]


def check_doc():
    """doc/rpc-heritage.md must agree with the in-code markers ROW BY ROW: every non-alias
    marked RPC has a doc row whose fp equals the marker fp (not merely present somewhere in
    the doc), and every doc row maps to a registered RPC (no stale/renamed rows)."""
    path = f"{REPO}/doc/rpc-heritage.md"
    if not os.path.exists(path):
        return ["doc/rpc-heritage.md is missing"]
    doc = open(path, encoding="utf-8", errors="replace").read()
    # per-row name (col 1) -> its fp from the fp column (table column 5 in the fingerprinted
    # tables: | RPC | file | args | result-keys | fp | [note] |). Taken positionally rather than
    # as "last code-span on the row" so a diverges note that happens to contain a 12-hex token
    # cannot be mistaken for the fp. None for the pure-gridcoin / name-collision tables (no fp
    # column). Split is escape-pipe-aware so a literal `\|` inside a cell does not shift columns.
    rows = {}
    for line in doc.splitlines():
        m = re.match(r'\|\s*`([a-z0-9]+)`\s*\|', line)
        if not m:
            continue
        cols = re.split(r'(?<!\\)\|', line)
        fp = None
        if len(cols) > 5:
            cm = re.search(r'`([0-9a-f]{12}|manual)`', cols[5])
            fp = cm.group(1) if cm else None
        rows[m.group(1)] = fp
    errs = []
    # Every registered RPC gets its own doc row (gen_doc lists aliases too), so check all of
    # them -- no alias skip, so a future fingerprinted alias can't escape doc/fp enforcement.
    for name in sorted(TABLE):
        label, fp = find_marker(name)
        if label is None:
            continue  # reported by main()
        if name not in rows:
            errs.append(f"{name}: classified but absent from doc/rpc-heritage.md")
        elif label in FINGERPRINTED and fp and rows[name] != fp:
            errs.append(f"{name}: doc fp={rows[name]} does not match the row's heritage_fp={fp} "
                        f"(per-row doc/table mismatch)")
    for name in sorted(rows):
        if name not in TABLE:
            errs.append(f"{name}: doc/rpc-heritage.md row has no registered RPC (stale/renamed?)")
    # The doc's per-bucket tally must equal the actual table buckets (no decorative counts).
    doc_tally = dict(re.findall(r'-\s*(pure-upstream|mixed|removed-upstream|pure-gridcoin)\b'
                                r'[^:\n]*:\s*\*\*(\d+)\*\*', doc))
    actual = {}
    for _label, _fp in HERITAGE.values():
        actual[_label] = actual.get(_label, 0) + 1
    for lbl, n in actual.items():
        if doc_tally.get(lbl) != str(n):
            errs.append(f"doc/rpc-heritage.md tally for {lbl} is {doc_tally.get(lbl, '<missing>')}, "
                        f"table has {n}")
    return errs


def check_result_builder_reachability():
    """Insurance for the bounded descent: surface() descends only to MAX_DESCENT, and it
    truncates a builder b for RPC R iff the SHORTEST path from R's impl to b exceeds the cap.
    Truncation is therefore PER-ROOT, so we BFS shortest hop-distances from each RPC root and
    keep, per builder, the MAX over roots. If any builder is reachable from some RPC deeper than
    MAX_DESCENT, that RPC's fingerprint silently drops its keys -> flag it (raise MAX_DESCENT or
    flatten the chain). Impl-builders are NOT exempted: one reached as a nested builder from a
    different RPC beyond the cap is truncated there just like any other builder.

    Scope: this walks the result-builder graph (pushKV/push_back functions). A builder reachable
    only through a helper that is itself NEITHER a pushKV nor a push_back function is invisible to
    BOTH this check and surface() -- a known residual limitation, not covered here."""
    B = builders()
    impls = {TABLE[n][0] for n in TABLE}
    worst = {}
    for i in impls:
        body = def_body(i, ("UniValue",))
        if not body:
            continue
        dist = {}
        dq = deque()
        for fn in _calls(body):  # builders called directly by this RPC = hop 1
            if fn in B and fn != i and dist.get(fn, 1 << 30) > 1:
                dist[fn] = 1
                dq.append(fn)
        while dq:
            b = dq.popleft()
            for fn in _calls(B[b]):
                if fn in B and fn != b and dist.get(fn, 1 << 30) > dist[b] + 1:
                    dist[fn] = dist[b] + 1
                    dq.append(fn)
        for b, d in dist.items():
            if d > worst.get(b, 0):
                worst[b] = d
    errs = []
    for b in sorted(worst):
        if worst[b] > MAX_DESCENT:
            errs.append(f"{b}(): result-builder reachable from an RPC at depth {worst[b]} > "
                        f"MAX_DESCENT={MAX_DESCENT} -> surface()'s recursive capture truncates its "
                        f"keys; raise MAX_DESCENT or flatten the builder chain")
    return errs


def main():
    errors = []
    for name in sorted(TABLE):
        label, fp = find_marker(name)
        if label is None:
            # Row parsed into TABLE but its heritage_<bucket> enum was unrecognized.
            errors.append(f"{name}: unrecognized heritage bucket in its vRPCCommands[] row")
            continue
        if label in FINGERPRINTED:
            uncap = output_uncapturable(name)
            if fp == "manual":
                if not uncap:
                    errors.append(f"{name}: heritage_fp=manual but the output IS literal-key-trackable "
                                  f"-> use a hash fp (recomputed {fingerprint(name)}), not manual")
                continue  # surface not machine-trackable; reviewed by hand
            if uncap:
                errors.append(f"{name}: output is not literal-key-trackable (dynamically-keyed object or "
                              f"positional array of scalars) -> set heritage_fp \"manual\", not a hash "
                              f"over an empty/partial surface")
                continue
            cur = fingerprint(name)
            if fp != cur:
                errors.append(f"{name}: surface fingerprint drift (row heritage_fp={fp or '<empty>'}, "
                              f"recomputed {cur}) -> re-confirm the heritage bucket and update the "
                              f"vRPCCommands[] row + doc/rpc-heritage.md")
        elif fp:  # pure-gridcoin must not carry a fingerprint
            errors.append(f"{name}: pure-gridcoin is not fingerprinted but its row has "
                          f"heritage_fp=\"{fp}\" -> set it to \"\"")
    errors += check_table_complete()
    errors += check_result_builder_reachability()
    errors += check_doc()
    if errors:
        print("RPC heritage lint FAILED:")
        for e in errors:
            print("  - " + e)
        return 1
    print(f"RPC heritage lint OK: {len(TABLE)} RPCs, all marked; fingerprints current.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
