#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""IPC auth-gate completeness lint (multiprocess design section 4.3).

src/ipc/serve_init.cpp wraps interfaces::Init and gates every method behind
RequireAuth(), so an unauthenticated IPC peer gets nothing. The wrapper is
hand-maintained, and C++ gives no warning when it falls out of step, because
both ways of getting it wrong compile cleanly:

  * A new virtual added to interfaces::Init with no override in ServeInit falls
    through to the base default. That fails CLOSED (the base returns nullptr /
    an empty value), which is correct but SILENT: the capability simply stops
    working over IPC with no diagnostic, and the obvious fix is to add the
    override -- which is the moment the RequireAuth() call gets forgotten.
  * An override added WITHOUT RequireAuth() fails OPEN. That one is a hole: the
    method is served to a peer that never presented the cookie.

This lint closes both. Every virtual on interfaces::Init must have an override
in ServeInit, and every override must call RequireAuth() -- except the two
methods listed in UNGATED below, which are exempt by design.

If you are here because the lint failed, the fix is almost always to add the
missing override to src/ipc/serve_init.cpp with RequireAuth() as its first
statement, delegating to m_inner.
"""

import re
import sys

INIT_HEADER = "src/interfaces/init.h"
SERVE_INIT = "src/ipc/serve_init.cpp"
INIT_CAPNP = "src/ipc/capnp/init.capnp"

# Methods that must NOT be gated, with the reason each is exempt.
UNGATED = {
    # The gate itself. Requiring auth to authenticate would deadlock the handshake.
    "authenticate",
    # Reports whether the gate has opened. Has to answer before authentication --
    # that is the only time the listener asks. Not exposed over capnp, so no remote
    # peer can call it.
    "isAuthenticated",
}

# Virtuals deliberately absent from init.capnp, with the reason each is exempt.
# Everything else on interfaces::Init MUST have an ordinal: without one, mpgen
# generates no override on ProxyClient<Init>, the call falls through to the base
# default (nullptr / an empty value), and the capability silently does not exist
# over IPC. That is not hypothetical -- the coin channel shipped that way and the
# multiprocess GUI could not start at all until wallet_coin_source.capnp was
# added.
UNSERVED = {
    # The listener asks this locally, before authentication, to decide whether a
    # connection beat the deadline. Serving it would let a peer probe the gate.
    "isAuthenticated",
}

# Schema methods with no interfaces::Init counterpart, with the reason.
NOT_A_VIRTUAL = {
    # libmultiprocess lifecycle entry point (the ThreadMap exchange), generated
    # by mpgen rather than declared on the C++ interface.
    "construct",
}

# "virtual <return type> <name>(" -- the return type may contain spaces, ::, <>, &, *.
VIRTUAL_RE = re.compile(r"\bvirtual\s+[\w:<>,\s*&]+?\s+(\w+)\s*\(([^)]*)\)")

# Line comments, block comments and string literals, so a mention of RequireAuth()
# in prose cannot satisfy the check below. Order matters: strings first would eat
# quotes inside comments.
COMMENT_RE = re.compile(r"//[^\n]*|/\*.*?\*/", re.DOTALL)
STRING_RE = re.compile(r'"(?:[^"\\]|\\.)*"')


def strip_noncode(text):
    """Remove comments and string literals, preserving newlines for line numbers."""
    def blank(m):
        return "".join(ch if ch == "\n" else " " for ch in m.group(0))
    return STRING_RE.sub(blank, COMMENT_RE.sub(blank, text))


def fail(msg):
    print("lint-serve-init-complete: {}".format(msg), file=sys.stderr)


def read(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return f.read()
    except OSError as e:
        fail("could not read {}: {}".format(path, e))
        return None


def class_body(text, class_name):
    """Return the source between the opening brace of `class_name` and its closer."""
    m = re.search(r"\bclass\s+{}\b[^{{;]*\{{".format(re.escape(class_name)), text)
    if not m:
        return None
    start = m.end()
    depth = 1
    i = start
    while i < len(text) and depth:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    return text[start:i - 1] if depth == 0 else None


def declared_virtuals(header_text):
    """Return [(name, arity), ...] for every virtual on interfaces::Init.

    Keyed by arity for the same reason overrides_with_bodies() is: keying by bare
    name made the completeness check fail open across overloads. Adding
    `Init::foo(int)` while ServeInit had only a gated `foo()` satisfied a
    name-only membership test, and the gate check then examined the existing
    zero-argument override and passed -- so the new overload reached IPC peers
    ungated, which is precisely what this lint exists to prevent.

    Matched over the whole class body rather than line by line so a declaration
    whose parameters wrap across lines still yields its arity; `[^)]*` spans
    newlines. Comments are stripped first so prose cannot introduce a phantom
    virtual.
    """
    body = class_body(header_text, "Init")
    if body is None:
        return None
    body = strip_noncode(body)
    out = []
    for m in VIRTUAL_RE.finditer(body):
        name = m.group(1)
        if name == "Init":  # the virtual destructor, written as "virtual ~Init()"
            continue
        params = m.group(2).strip()
        arity = 0 if not params else params.count(",") + 1
        out.append((name, arity))
    return out


def overrides_with_bodies(serve_text):
    """Map override name -> body text, for every `... name(...) override { ... }`."""
    body = class_body(serve_text, "ServeInit")
    if body is None:
        return None
    body = strip_noncode(body)
    out = {}
    for m in re.finditer(r"\b(\w+)\s*\(([^)]*)\)\s*(?:const\s*)?override\b[^{]*\{", body):
        name = m.group(1)
        # Key by name AND parameter count: keying by bare name let an ungated
        # overload silently overwrite (or be overwritten by) the gated one, so the
        # ungated member was never checked. Overloads are checked independently.
        params = m.group(2).strip()
        arity = 0 if not params else params.count(",") + 1
        key = (name, arity)
        start = m.end()
        depth = 1
        i = start
        while i < len(body) and depth:
            if body[i] == "{":
                depth += 1
            elif body[i] == "}":
                depth -= 1
            i += 1
        out[key] = body[start:i - 1]
    return out


def capnp_init_methods(capnp_text):
    """Return {name: ordinal} for the methods of `interface Init` in init.capnp.

    Scoped to that interface's braces rather than the whole file: the nested
    structs (BuildInfo, NodeIdentity) restart their own ordinal spaces, and a
    file-wide scan would mix their FIELD names in with the interface's methods.
    """
    m = re.search(r"\binterface\s+Init\b[^{]*\{", capnp_text)
    if not m:
        return None
    start = m.end()
    depth = 1
    i = start
    while i < len(capnp_text) and depth:
        if capnp_text[i] == "{":
            depth += 1
        elif capnp_text[i] == "}":
            depth -= 1
        i += 1
    if depth:
        return None
    body = capnp_text[start:i - 1]
    # Strip capnp comments so prose cannot introduce a phantom method.
    body = re.sub(r"#[^\n]*", "", body)
    return {mm.group(1): int(mm.group(2))
            for mm in re.finditer(r"^\s*(\w+)\s*@(\d+)\s*\(", body, re.M)}


def main():
    header_text = read(INIT_HEADER)
    serve_text = read(SERVE_INIT)
    capnp_text = read(INIT_CAPNP)
    if header_text is None or serve_text is None or capnp_text is None:
        return 1

    expected = declared_virtuals(header_text)
    if expected is None:
        fail("could not locate `class Init` in {} -- the lint needs updating".format(INIT_HEADER))
        return 1
    if not expected:
        fail("found no virtual methods on interfaces::Init; the parser is probably broken")
        return 1

    found = overrides_with_bodies(serve_text)
    if found is None:
        fail("could not locate `class ServeInit` in {} -- the lint needs updating".format(SERVE_INIT))
        return 1

    errors = []

    # Compare (name, arity) keys on BOTH sides. Every declared overload must have
    # its own gated override; matching on the bare name let a new overload borrow
    # the verdict of an existing one.
    expected_keys = set(expected)

    for key in expected:
        name, arity = key
        if name in UNGATED:
            continue
        if key not in found:
            errors.append(
                "interfaces::Init::{0}() (the {1}-argument overload) has no override in ServeInit "
                "({2}). It would fall through to the base default and silently return nothing over "
                "IPC. Add an override that calls RequireAuth() and delegates to m_inner.".format(
                    name, arity, SERVE_INIT))
        elif "RequireAuth()" not in found[key]:
            errors.append(
                "ServeInit::{0}() (the {1}-argument overload) does not call RequireAuth() "
                "({2}). It would be served to an unauthenticated IPC peer.".format(
                    name, arity, SERVE_INIT))

    # An override with no counterpart on the interface is dead code, and usually
    # means a rename landed on one side only. Arity-aware too: an override whose
    # signature no longer matches any declaration overrides nothing.
    for key in found:
        if key not in expected_keys:
            errors.append(
                "ServeInit::{0}() (the {1}-argument overload) overrides nothing declared on "
                "interfaces::Init ({2}); it is dead code or a half-finished rename.".format(
                    key[0], key[1], INIT_HEADER))

    # --- Third gate: the capnp schema ---
    #
    # An override in ServeInit only matters if the call can reach it. mpgen drives
    # ProxyClient<Init> from init.capnp, so a virtual with no ordinal there is never
    # overridden client-side: the call resolves to interfaces::Init's own default,
    # returns nullptr, and never leaves the GUI process. Nothing else catches this.
    # The compiler cannot -- overriding a defaulted virtual is optional.
    # lint-capnp-schema-compat.py compares ordinals that EXIST against a released
    # baseline, so it cannot miss one that was never added. The multiprocess CI jobs
    # build; they do not run a GUI. And INTERFACES_ASSERT_MARSHALABLE asserts the
    # DTOs are copyable value types, which says nothing about whether marshalling
    # exists -- the coin channel carried fifteen of those assertions while having no
    # schema at all.
    capnp_methods = capnp_init_methods(capnp_text)
    if capnp_methods is None:
        fail("could not locate `interface Init` in {} -- the lint needs updating".format(INIT_CAPNP))
        return 1
    if not capnp_methods:
        fail("found no methods on `interface Init` in {}; the parser is probably broken".format(INIT_CAPNP))
        return 1

    # Overloads are rejected outright, because Cap'n Proto cannot represent them:
    # method names must be unique within an interface scope ("error: 'bar' is
    # already defined in this scope"). An overloaded virtual on an IPC-served
    # interface is therefore never fully wirable -- at most one of the overloads
    # can have an ordinal, and the others are silently unreachable over IPC no
    # matter what anyone intends. No interfaces:: class has one today.
    #
    # That structural fact is also why this gate matches by NAME and does not try
    # to disambiguate by arity. It could not: a capnp method's input parameters do
    # not correspond to the C++ parameter list, because out-parameters live in the
    # RESULTS. interfaces::Wallet::getNewReceiveAddress(std::string&) is C++ arity
    # 1 with ZERO capnp inputs besides context, and getAddressLabel(const
    # std::string&, std::string&) is arity 2 with one. Deriving arity from the
    # schema would reject correct code as soon as Init gained such a method.
    #
    # So the name match is safe precisely BECAUSE overloads are refused here.
    seen_names = set()
    overloaded = set()
    for name, _arity in expected:
        if name in seen_names:
            overloaded.add(name)
        seen_names.add(name)
    for name in sorted(overloaded):
        errors.append(
            "interfaces::Init::{0}() is overloaded ({1}). Cap'n Proto requires method names to "
            "be unique within an interface, so an overload cannot be represented in {2} at all: "
            "at most one of them can hold an ordinal and the rest are unreachable over IPC "
            "however they are declared. Give the overloads distinct names.".format(
                name, INIT_HEADER, INIT_CAPNP))

    for name, _arity in expected:
        if name in UNSERVED:
            continue
        if name in overloaded:
            continue  # already reported above; the name check below cannot be trusted
        if name not in capnp_methods:
            errors.append(
                "interfaces::Init::{0}() has no ordinal in {1}. mpgen therefore generates no "
                "override on ProxyClient<Init>, so over IPC the call falls through to the base "
                "default and the capability silently does not exist -- the multiprocess GUI sees "
                "nullptr. Add `{0} @<next> (context :Proxy.Context) -> (result :...);` to "
                "`interface Init`, with a schema for the returned interface if it has none yet. "
                "If it is deliberately not served, add it to UNSERVED in this lint with the "
                "reason.".format(name, INIT_CAPNP))

    # The reverse: an ordinal with no virtual behind it. mpgen would fail to build,
    # but naming it here reports the cause rather than a template error.
    declared_names = {name for name, _ in expected}
    for name in capnp_methods:
        if name in NOT_A_VIRTUAL:
            continue
        if name not in declared_names:
            errors.append(
                "{0} declares `{1}` on `interface Init`, but interfaces::Init has no such virtual "
                "({2}); it is a stale ordinal or a half-finished rename.".format(
                    INIT_CAPNP, name, INIT_HEADER))

    # An UNSERVED entry that IS in the schema is a contradiction worth catching.
    for name in UNSERVED:
        if name in capnp_methods:
            errors.append(
                "interfaces::Init::{0}() is listed as unserved-by-design but has an ordinal in "
                "{1}; update either the schema or UNSERVED in this lint.".format(name, INIT_CAPNP))

    # An UNGATED entry that calls RequireAuth() is a contradiction worth catching.
    for name in UNGATED:
        if any(k[0] == name and "RequireAuth()" in b for k, b in found.items()):
            errors.append(
                "ServeInit::{0}() is listed as ungated-by-design but calls RequireAuth(); update "
                "either the code or UNGATED in this lint.".format(name))

    if errors:
        for e in errors:
            fail(e)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
