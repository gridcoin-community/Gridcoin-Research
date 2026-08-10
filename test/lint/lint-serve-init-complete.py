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
VIRTUAL_RE = re.compile(r"^\s*virtual\s+[\w:<>,\s*&]+?\s+(\w+)\s*\(")


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
    body = class_body(header_text, "Init")
    if body is None:
        return None
    names = []
    for line in body.splitlines():
        m = VIRTUAL_RE.match(line)
        if not m:
            continue
        name = m.group(1)
        if name == "Init":  # the virtual destructor, written as "virtual ~Init()"
            continue
        names.append(name)
    return names


def overrides_with_bodies(serve_text):
    """Map override name -> body text, for every `... name(...) override { ... }`."""
    body = class_body(serve_text, "ServeInit")
    if body is None:
        return None
    out = {}
    for m in re.finditer(r"\b(\w+)\s*\([^)]*\)\s*override\s*\{", body):
        name = m.group(1)
        start = m.end()
        depth = 1
        i = start
        while i < len(body) and depth:
            if body[i] == "{":
                depth += 1
            elif body[i] == "}":
                depth -= 1
            i += 1
        out[name] = body[start:i - 1]
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

    for name in expected:
        if name in UNGATED:
            continue
        if name not in found:
            errors.append(
                "interfaces::Init::{0}() has no override in ServeInit ({1}). It would fall through "
                "to the base default and silently return nothing over IPC. Add an override that "
                "calls RequireAuth() and delegates to m_inner.".format(name, SERVE_INIT))
        elif "RequireAuth()" not in found[name]:
            errors.append(
                "ServeInit::{0}() does not call RequireAuth() ({1}). It would be served to an "
                "unauthenticated IPC peer.".format(name, SERVE_INIT))

    # An override with no counterpart on the interface is dead code, and usually
    # means a rename landed on one side only.
    for name in found:
        if name not in expected:
            errors.append(
                "ServeInit::{0}() overrides nothing declared on interfaces::Init ({1}); it is dead "
                "code or a half-finished rename.".format(name, INIT_HEADER))

    # An UNGATED entry that calls RequireAuth() is a contradiction worth catching.
    for name in UNGATED:
        if name in found and "RequireAuth()" in found[name]:
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
