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

# Methods that must NOT be gated, with the reason each is exempt.
UNGATED = {
    # The gate itself. Requiring auth to authenticate would deadlock the handshake.
    "authenticate",
    # Reports whether the gate has opened. Has to answer before authentication --
    # that is the only time the listener asks. Not exposed over capnp, so no remote
    # peer can call it.
    "isAuthenticated",
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


def main():
    header_text = read(INIT_HEADER)
    serve_text = read(SERVE_INIT)
    if header_text is None or serve_text is None:
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
