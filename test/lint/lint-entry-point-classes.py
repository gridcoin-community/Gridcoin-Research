#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Entry-point access classes lint.

src/rpc/entry_point_classes.json gives every RPC row of src/rpc/server.cpp and
every interface method of src/ipc/capnp/*.capnp an access class and an effect:

    {"ipc": {"Interface.method": ENTRY, ...}, "rpc": {"name": ENTRY, ...}}
    ENTRY = {"class": C, "effect": E} plus the optional fields "kind",
            "authority", "path", "minted_by" and "note"

doc/developer-notes.md ("Entry-point access classes") says what each class
means. This lint keeps the file true against the code:

   1. Every vRPCCommands row has an "rpc" entry and every "rpc" entry has a
      row. The row's access column matches the class: access_elevate_inband
      for elevate with authority passphrase_argument, access_elevate for any
      other elevate, access_<class> otherwise. RPC classes are read_only,
      free, elevate and settings, and an RPC entry has no kind or minted_by.
   2. Every capnp interface method has an "ipc" entry and every "ipc" entry
      has a method. Method declarations are also counted without regard to
      the name, so one the parser cannot read is reported.
   3. The class, the effect and the authority are known values, and only an
      elevate entry has an authority.
   4. An entry whose effect is value, keys or key_writer is of class elevate,
      settings, dispatch, elevated_handle or refuse.
   5. An entry whose effect is read_only is of class read_only, or of class
      free with "path": true; an entry of class read_only has effect
      read_only and no path.
   6. An "ipc" entry of class elevate without an authority names, in "kind",
      an enumerator of enum class CapabilityKind. No other entry has a kind.
   7. Class dispatch is only on Node.executeRpcConsoleCommand and
      Node.executeRpcConsoleSecretCommand; class settings only on the RPC
      changesettings and Node.changeSettings.
   8. The implementing method of an "ipc" elevate entry with a kind passes
      CapabilityKind::<kind> as an argument of the capability it opens (a
      CapabilityScope declaration or temporary, an .emplace() of one, or
      RunUnderIpcCapability()), and Node.changeSettings does so with
      CapabilityKind::Settings. The method is the one definition of that name
      in the implementing class: a name not preceded by '.', '->' or '::',
      whose parameter list is followed only by const, override, final,
      noexcept, & or && before its '{'. IMPLEMENTATIONS maps each interface
      to its file and class; a new interface needs a line there before such
      an entry is accepted. EXEMPT lists the methods excused, with the reason.
   9. An elevated_handle entry lists in "minted_by" the "ipc" entries of class
      elevate that make its object. No other entry has minted_by.
  10. The key entry point list (--key-entry-points, by default
      src/test/data/key_entry_points.json) exists, has non-empty "rpc" and
      "ipc" lists, and every RPC and Interface.method in them is of class
      elevate or dispatch here. A dispatch entry reaches the keys only
      through the RPC it carries, whose own row rule 1 and the dispatcher
      gate hold to elevate, and rule 7 confines dispatch to the console
      methods.
  11. The file is formatted as json.dumps(data, indent=2, sort_keys=True)
      plus a newline.

Each failure prints "lint-entry-point-classes: <entry>: <reason>".
"""

import argparse
import glob
import json
import os
import re
import sys

CLASSES_FILE = "src/rpc/entry_point_classes.json"
SERVER_FILE = "src/rpc/server.cpp"
CAPNP_GLOB = "src/ipc/capnp/*.capnp"
HEADER_GLOB = "src/**/*.h"
KEY_ENTRY_POINTS_FILE = "src/test/data/key_entry_points.json"

CLASSES = ("read_only", "free", "elevate", "settings", "dispatch", "elevated_handle", "refuse")
RPC_CLASSES = ("read_only", "free", "elevate", "settings")
EFFECTS = ("read_only", "state", "value", "keys", "key_writer")
AUTHORITIES = ("passphrase_argument", "unencrypted_only")
FIELDS = ("class", "effect", "kind", "authority", "path", "minted_by", "note")

# Rule 4: the classes that may carry an effect of value, keys or key_writer.
GUARDED_CLASSES = ("elevate", "settings", "dispatch", "elevated_handle", "refuse")
GUARDED_EFFECTS = ("value", "keys", "key_writer")

# Rule 10: the classes an entry point that reaches the keys may have.
KEY_REACH_CLASSES = ("elevate", "dispatch")

# Rule 7.
DISPATCH_ENTRIES = ("ipc.Node.executeRpcConsoleCommand", "ipc.Node.executeRpcConsoleSecretCommand")
SETTINGS_ENTRIES = ("rpc.changesettings", "ipc.Node.changeSettings")

# Rule 8: interface -> (file, class that implements it).
IMPLEMENTATIONS = {
    "Wallet": ("src/wallet/interfaces.cpp", "WalletImpl"),
    "Node": ("src/node/interfaces.cpp", "NodeImpl"),
    "MRC": ("src/gridcoin/interfaces.cpp", "MRCImpl"),
    "SideStakeManager": ("src/gridcoin/interfaces.cpp", "SideStakeManagerImpl"),
    "VotingManager": ("src/gridcoin/interfaces.cpp", "VotingManagerImpl"),
    "ResearcherContext": ("src/gridcoin/interfaces.cpp", "ResearcherContextImpl"),
    "PSGTPoolContext": ("src/gridcoin/interfaces.cpp", "PSGTPoolContextImpl"),
}

# Rule 8: (interface, method) -> the reason its body need not name its kind.
EXEMPT = {
}

# The vRPCCommands row, as test/lint/lint-rpc-heritage.py parses it, with the
# access column captured:
#   { "name", &impl, cat_xxx, &help_helpman, heritage_<bucket>, "<fp>", access_<class> }
ROW_RE = re.compile(r'\{\s*"([a-z0-9]+)"\s*,\s*&(\w+)\s*,\s*\w+\s*,\s*&(\w+)\s*,'
                    r'\s*heritage_(\w+)\s*,\s*"([0-9a-f]{12}|manual|)"\s*,\s*access_(\w+)\s*\}')
# Any row opener, counted without regard to the name, so that a row ROW_RE
# cannot parse is reported instead of being skipped.
ROW_CANDIDATE_RE = re.compile(r'^\s*\{\s*"[^"]+"\s*,\s*&', re.M)

# The capnp parse of test/lint/lint-capnp-context.py.
MEMBER_RE = re.compile(r"(?<![\w.])([a-zA-Z][a-zA-Z0-9_]*)\s*@(\d+)\s*\(", re.MULTILINE)
SCOPE_RE = re.compile(r"\b(interface|struct|enum|union)\s+(\w+)[^{]*\{")
CAPNP_COMMENT_RE = re.compile(r"#[^\n]*")
# Any method declaration, counted without regard to the name: an ordinal
# followed by a parameter list or a generic parameter list.
CAPNP_CANDIDATE_RE = re.compile(r"@\d+\s*[\[(]")

CAPABILITY_ENUM_RE = re.compile(r"\benum\s+class\s+CapabilityKind\b[^{;]*\{([^}]*)\}")

# Rule 8: the calls that open a capability. The kind must be one of the call's
# own arguments: a CapabilityScope declaration (scope(...) or scope{...}) or
# temporary, an emplace() into a std::optional<CapabilityScope>, or
# RunUnderIpcCapability(wallet, kind, fn).
CAPABILITY_CALL_RE = re.compile(r"\bCapabilityScope\s*(?:[A-Za-z_]\w*\s*)?[({]"
                                r"|\.\s*emplace\s*\("
                                r"|\bRunUnderIpcCapability\s*\(")
# Rule 8: what may stand between a definition's parameter list and its body.
DEFINITION_TAIL_RE = re.compile(r"(?:\s|\bconst\b|\boverride\b|\bfinal\b|\bnoexcept\b|&&|&)*")

errors = []


def fail(entry, reason):
    errors.append("lint-entry-point-classes: {}: {}".format(entry, reason))


def read_text(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def blank_comments_and_literals(text):
    """Blank out // and /* */ comments and the contents of string and character
    literals, keeping offsets. A kind named only in a comment or a string does
    not count, and a brace in either does not upset the brace matching. (Raw
    string literals are not recognised; the files read here have none.)"""
    out = list(text)
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            for k in range(i + 1, min(j, n)):
                out[k] = " "
            i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != "\n":
                    out[k] = " "
            i = j
        else:
            i += 1
    return "".join(out)


def brace_block(text, start):
    """The text from the first '{' at or after start through its matching '}'."""
    i = text.find("{", start)
    if i < 0:
        return ""
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
    return text[i:]


def rpc_rows():
    """name -> access class of every vRPCCommands row."""
    text = read_text(SERVER_FILE)
    begin = text.find("vRPCCommands[]")
    if begin < 0:
        return {}
    seg = text[begin:text.find("\n};", begin)]
    rows = {}
    parsed = 0
    for m in ROW_RE.finditer(seg):
        parsed += 1
        if m.group(1) in rows:
            fail("rpc." + m.group(1), "vRPCCommands has more than one row of this name")
        rows[m.group(1)] = m.group(6)
    candidates = len(ROW_CANDIDATE_RE.findall(seg))
    if parsed < candidates:
        fail(SERVER_FILE, "{} vRPCCommands row(s) did not parse (name or column shape); "
                          "they are not checked".format(candidates - parsed))
    return rows


def innermost_scope(spans, pos):
    """(kind, name) of the tightest capnp scope containing pos, or None."""
    best = None
    best_start = -1
    for kind, name, start, end in spans:
        if start <= pos < end and start > best_start:
            best, best_start = (kind, name), start
    return best


def capnp_methods():
    """Interface.method of every capnp interface method."""
    methods = set()
    parsed = 0
    candidates = 0
    for path in sorted(glob.glob(CAPNP_GLOB)):
        text = CAPNP_COMMENT_RE.sub(lambda m: " " * len(m.group(0)), read_text(path))
        spans = []
        for m in SCOPE_RE.finditer(text):
            depth = 1
            i = m.end()
            while i < len(text) and depth:
                if text[i] == "{":
                    depth += 1
                elif text[i] == "}":
                    depth -= 1
                i += 1
            if depth == 0:
                spans.append((m.group(1), m.group(2), m.end(), i - 1))
        for m in CAPNP_CANDIDATE_RE.finditer(text):
            scope = innermost_scope(spans, m.start())
            if scope is not None and scope[0] == "interface":
                candidates += 1
        for m in MEMBER_RE.finditer(text):
            best = innermost_scope(spans, m.start())
            if best is None or best[0] != "interface":
                continue  # a struct field or an enumerant, not a method
            parsed += 1
            key = "{}.{}".format(best[1], m.group(1))
            if key in methods:
                fail("ipc." + key, "more than one capnp method of this name")
            methods.add(key)
    if candidates > parsed:
        fail(CAPNP_GLOB, "{} interface method declaration(s) did not parse (name or shape); "
                         "they are not checked".format(candidates - parsed))
    return methods


def capability_kinds():
    """The enumerators of enum class CapabilityKind, from the header that declares it."""
    found = []
    for path in sorted(glob.glob(HEADER_GLOB, recursive=True)):
        text = read_text(path)
        if "CapabilityKind" not in text:
            continue
        m = CAPABILITY_ENUM_RE.search(blank_comments_and_literals(text))
        if m:
            found.append((path, m.group(1)))
    if len(found) != 1:
        fail("CapabilityKind", "declared in {} headers, expected one: {}".format(
            len(found), ", ".join(p for p, _ in found) or "none"))
        return set()
    kinds = set()
    for item in found[0][1].split(","):
        m = re.match(r"\s*([A-Za-z_]\w*)", item)
        if m:
            kinds.add(m.group(1))
    return kinds


def matching_close(text, open_pos):
    """The index of the bracket that closes the one at open_pos, counting
    (), [] and {} together, or -1."""
    depth = 0
    for i in range(open_pos, len(text)):
        if text[i] in "([{":
            depth += 1
        elif text[i] in ")]}":
            depth -= 1
            if depth == 0:
                return i
    return -1


def method_bodies(text, cls, method):
    """The bodies of every definition of method in the block of class cls, or
    None when the class is not found. A definition is an occurrence of
    'method(' not preceded by '.', '->' or '::' whose parameter list is
    followed only by const, override, final, noexcept, & or && before '{'."""
    m = re.search(r"\bclass\s+{}\b[^;{{]*\{{".format(re.escape(cls)), text)
    if not m:
        return None
    block = brace_block(text, m.start())
    bodies = []
    for name in re.finditer(r"\b{}\s*\(".format(re.escape(method)), block):
        before = block[:name.start()].rstrip()
        if before.endswith(".") or before.endswith("->") or before.endswith("::"):
            continue  # a call through an object or a qualified name
        close = matching_close(block, name.end() - 1)
        if close < 0:
            continue
        tail = DEFINITION_TAIL_RE.match(block, close + 1)
        if tail.end() < len(block) and block[tail.end()] == "{":
            bodies.append(brace_block(block, tail.end()))
    return bodies


def top_level_arguments(text, open_pos):
    """The comma-separated arguments of the call whose bracket is at open_pos."""
    close = matching_close(text, open_pos)
    if close < 0:
        return []
    args = []
    depth = 0
    start = open_pos + 1
    for i in range(open_pos + 1, close):
        if text[i] in "([{":
            depth += 1
        elif text[i] in ")]}":
            depth -= 1
        elif text[i] == "," and depth == 0:
            args.append(text[start:i].strip())
            start = i + 1
    args.append(text[start:close].strip())
    return args


def opens_capability(body, kind):
    """Whether body passes CapabilityKind::<kind> as an argument of a call
    that opens a capability."""
    want = "CapabilityKind::" + kind
    for call in CAPABILITY_CALL_RE.finditer(body):
        if want in top_level_arguments(body, call.end() - 1):
            return True
    return False


def load_classes():
    def no_duplicates(pairs):
        out = {}
        for key, value in pairs:
            if key in out:
                raise ValueError("key '{}' appears twice".format(key))
            out[key] = value
        return out

    raw = read_text(CLASSES_FILE)
    try:
        data = json.loads(raw, object_pairs_hook=no_duplicates)
    except ValueError as e:
        fail(CLASSES_FILE, "not valid JSON: {}".format(e))
        return None
    if not isinstance(data, dict) or sorted(data) != ["ipc", "rpc"] \
            or not all(isinstance(data[k], dict) for k in data):
        fail(CLASSES_FILE, 'the file must be an object with exactly the objects "ipc" and "rpc"')
        return None
    # Rule 11.
    if raw != json.dumps(data, indent=2, sort_keys=True) + "\n":
        fail(CLASSES_FILE, "not in canonical form; rewrite it as "
                           "json.dumps(data, indent=2, sort_keys=True) + a newline")
    return data


def check_entry(name, entry, is_rpc, kinds):
    """Rules 3 to 7 and the field shapes for one entry."""
    if not isinstance(entry, dict):
        fail(name, "the entry is not an object")
        return
    for field in sorted(entry):
        if field not in FIELDS:
            fail(name, "unknown field '{}'".format(field))
    cls = entry.get("class")
    effect = entry.get("effect")
    if cls not in CLASSES:
        fail(name, "class {!r} is not one of {}".format(cls, ", ".join(CLASSES)))
    elif is_rpc and cls not in RPC_CLASSES:
        fail(name, "class {} is not an RPC class ({})".format(cls, ", ".join(RPC_CLASSES)))
    if effect not in EFFECTS:
        fail(name, "effect {!r} is not one of {}".format(effect, ", ".join(EFFECTS)))
    if "authority" in entry:
        if cls != "elevate":
            fail(name, "authority is only for class elevate")
        elif entry["authority"] not in AUTHORITIES:
            fail(name, "authority {!r} is not one of {}".format(entry["authority"], ", ".join(AUTHORITIES)))
    if "path" in entry and entry["path"] is not True:
        fail(name, "path, when present, is true")
    if "note" in entry and (not isinstance(entry["note"], str) or not entry["note"].strip()):
        fail(name, "note, when present, is a non-empty string")

    # Rule 4.
    if effect in GUARDED_EFFECTS and cls not in GUARDED_CLASSES:
        fail(name, "effect {} needs class {}, not {}".format(effect, ", ".join(GUARDED_CLASSES), cls))
    # Rule 5, both ways.
    if effect == "read_only" and not (cls == "read_only" or (cls == "free" and entry.get("path") is True)):
        fail(name, "effect read_only needs class read_only, or class free with \"path\": true")
    if cls == "read_only" and (effect != "read_only" or "path" in entry):
        fail(name, "class read_only needs effect read_only and no path")

    # Rule 6, and rule 1's "no kind on an RPC entry".
    wants_kind = not is_rpc and cls == "elevate" and "authority" not in entry
    if wants_kind:
        if "kind" not in entry:
            fail(name, "an elevate IPC method without an authority names its CapabilityKind in \"kind\"")
        elif not isinstance(entry["kind"], str) or entry["kind"] not in kinds:
            fail(name, "kind {!r} is not an enumerator of CapabilityKind ({})".format(
                entry["kind"], ", ".join(sorted(kinds))))
    elif "kind" in entry:
        fail(name, "an RPC entry has no kind" if is_rpc else
             "only an elevate IPC method without an authority has a kind")

    # Rule 9's "no other entry", and rule 1's "no minted_by on an RPC entry".
    if "minted_by" in entry and cls != "elevated_handle":
        fail(name, "only an elevated_handle entry has minted_by")

    # Rule 7.
    if cls == "dispatch" and name not in DISPATCH_ENTRIES:
        fail(name, "class dispatch is only for {}".format(", ".join(DISPATCH_ENTRIES)))
    if cls == "settings" and name not in SETTINGS_ENTRIES:
        fail(name, "class settings is only for {}".format(", ".join(SETTINGS_ENTRIES)))


def expected_column(entry):
    """Rule 1: the access column the row of an RPC entry carries."""
    cls = entry.get("class")
    if cls == "elevate" and entry.get("authority") == "passphrase_argument":
        return "elevate_inband"
    return cls


def check_rpc(classes, rows):
    for name in sorted(set(rows) - set(classes)):
        fail("rpc." + name, "the RPC row has no entry in " + CLASSES_FILE)
    for name in sorted(set(classes) - set(rows)):
        fail("rpc." + name, "the entry has no vRPCCommands row")
    for name in sorted(set(rows) & set(classes)):
        entry = classes[name]
        if not isinstance(entry, dict):
            continue
        want = expected_column(entry)
        if want in RPC_CLASSES + ("elevate_inband",) and rows[name] != want:
            fail("rpc." + name, "the row's column is access_{} but class {}{} needs access_{}".format(
                rows[name], entry.get("class"),
                " with authority " + entry["authority"] if "authority" in entry else "", want))


def check_ipc(classes, methods):
    for key in sorted(methods - set(classes)):
        fail("ipc." + key, "the capnp method has no entry in " + CLASSES_FILE)
    for key in sorted(set(classes) - methods):
        fail("ipc." + key, "the entry has no capnp method")


def check_bodies(classes):
    """Rule 8."""
    texts = {}
    for key in sorted(classes):
        entry = classes[key]
        if not isinstance(entry, dict):
            continue
        if entry.get("class") == "elevate" and isinstance(entry.get("kind"), str):
            kind = entry["kind"]
        elif key == "Node.changeSettings":
            kind = "Settings"
        else:
            continue
        interface, _, method = key.partition(".")
        if (interface, method) in EXEMPT:
            continue
        if interface not in IMPLEMENTATIONS:
            fail("ipc." + key, "interface {} is not in this lint's IMPLEMENTATIONS map; add the file "
                               "and class that implement it".format(interface))
            continue
        path, cls = IMPLEMENTATIONS[interface]
        if path not in texts:
            texts[path] = blank_comments_and_literals(read_text(path))
        bodies = method_bodies(texts[path], cls, method)
        if bodies is None:
            fail("ipc." + key, "class {} not found in {}".format(cls, path))
        elif not bodies:
            fail("ipc." + key, "no definition of {}::{} found in {}".format(cls, method, path))
        elif len(bodies) > 1:
            fail("ipc." + key, "{} definitions of {}::{} in {}; rule 8 cannot tell which implements the "
                               "method (add it to EXEMPT with the reason if this is intended)"
                               .format(len(bodies), cls, method, path))
        elif not opens_capability(bodies[0], kind):
            fail("ipc." + key, "{}::{} in {} does not pass CapabilityKind::{} to a CapabilityScope, an "
                               "emplace() of one or RunUnderIpcCapability()".format(cls, method, path, kind))


def check_handles(classes):
    """Rule 9."""
    for key in sorted(classes):
        entry = classes[key]
        if not isinstance(entry, dict) or entry.get("class") != "elevated_handle":
            continue
        minted_by = entry.get("minted_by")
        if not isinstance(minted_by, list) or not minted_by:
            fail("ipc." + key, "an elevated_handle entry lists the methods that mint it in minted_by")
            continue
        for minter in minted_by:
            target = classes.get(minter) if isinstance(minter, str) else None
            if not isinstance(target, dict) or target.get("class") != "elevate":
                fail("ipc." + key, "minted_by names {!r}, which is not an IPC entry of class elevate"
                                   .format(minter))


def check_key_entry_points(path, data):
    """Rule 10."""
    try:
        listing = json.loads(read_text(path))
        for field in ("rpc", "ipc"):
            if not isinstance(listing.get(field), list) or not listing[field]:
                fail(path, "has no non-empty \"{}\" list".format(field))
                return
        rpc = [row["name"] for row in listing["rpc"]]
        ipc = ["{}.{}".format(row["interface"], row["method"]) for row in listing["ipc"]]
    except (ValueError, KeyError, TypeError, AttributeError) as e:
        fail(path, "cannot read its rpc and ipc lists: {}".format(e))
        return
    for table, names in (("rpc", rpc), ("ipc", ipc)):
        for name in names:
            entry = data[table].get(name) if isinstance(name, str) else None
            cls = entry.get("class") if isinstance(entry, dict) else None
            if cls not in KEY_REACH_CLASSES:
                fail("{}.{}".format(table, name), "{} lists it as reaching the keys, so its class must be {}, "
                     "not {}".format(path, " or ".join(KEY_REACH_CLASSES),
                                     cls if entry is not None else "absent (it has no entry here)"))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--key-entry-points", metavar="PATH",
                        help="the key entry point list for rule 10 (default: {})".format(KEY_ENTRY_POINTS_FILE))
    args = parser.parse_args()
    key_entry_points = os.path.abspath(args.key_entry_points) if args.key_entry_points else None

    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

    if not os.path.exists(CLASSES_FILE):
        print("lint-entry-point-classes: missing " + CLASSES_FILE, file=sys.stderr)
        return 1

    rows = rpc_rows()
    methods = capnp_methods()
    kinds = capability_kinds()
    if not rows:
        fail(SERVER_FILE, "parsed no vRPCCommands rows; the parser is probably broken")
    if not methods:
        fail(CAPNP_GLOB, "parsed no interface methods; the parser is probably broken")
    if not kinds and not errors:
        fail("CapabilityKind", "parsed no enumerators; the parser is probably broken")

    data = load_classes()
    if data is not None:
        for name in sorted(data["rpc"]):
            check_entry("rpc." + name, data["rpc"][name], True, kinds)
        for key in sorted(data["ipc"]):
            check_entry("ipc." + key, data["ipc"][key], False, kinds)
        check_rpc(data["rpc"], rows)
        check_ipc(data["ipc"], methods)
        check_bodies(data["ipc"])
        check_handles(data["ipc"])

        key_entry_points = key_entry_points or KEY_ENTRY_POINTS_FILE
        if not os.path.exists(key_entry_points):
            fail(key_entry_points, "no such file; rule 10 needs the key entry point list")
        else:
            check_key_entry_points(key_entry_points, data)

    if errors:
        for e in errors:
            print(e, file=sys.stderr)
        return 1

    print("lint-entry-point-classes: OK: {} RPC rows and {} IPC methods classified".format(
        len(rows), len(methods)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
