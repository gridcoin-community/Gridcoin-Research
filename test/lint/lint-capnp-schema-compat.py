#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Cap'n Proto schema backward-compatibility lint (multiprocess, design section 4.2).

The IPC wire format is defined by the src/ipc/capnp/*.capnp schemas. Two distinct
kinds of stability matter, and this lint guards both:

  * Wire compatibility depends on the file id (@0x...) and the ordinals (@N)
    with their types: renumbering, reusing, removing, or changing the type of a
    released ordinal makes an old peer and a new one disagree on the wire.
  * Member *names* do NOT affect the wire -- renaming a field/method while
    keeping its ordinal is wire-compatible. But the name IS the generated C++
    proxy API (and the $Proxy.name mapping to the C++ member), so a rename is a
    source-level break for everything that uses the proxies.

This lint diffs each schema against its version at the most recent mainnet
release tag and fails if a released ordinal was removed or renamed, or if a
file's id changed. (It does not yet diff field types; type changes on an existing
ordinal are a wire break it would miss -- a future enhancement.)

Schemas that did not exist at the baseline release are new: there is nothing to
compare, so they pass (their ordinals become the baseline at the next release).
If no baseline release tag is found, the lint passes (nothing to compare against).
"""

import os
import re
import subprocess
import sys

CAPNP_DIR = "src/ipc/capnp"
# Match a pure mainnet release tag: N.N.N.N (no -testnet / -macos / other suffix).
RELEASE_TAG_RE = re.compile(r"^\d+\.\d+\.\d+\.\d+$")
FILE_ID_RE = re.compile(r"^\s*@(0x[0-9a-fA-F]+)\s*;")
SCOPE_RE = re.compile(r"^\s*(?:interface|struct)\s+(\w+)")
# A member with an ordinal: "name @N ..." (methods and fields both).
MEMBER_RE = re.compile(r"^\s*(\w+)\s*@(\d+)\b")


def sh(args):
    return subprocess.run(args, capture_output=True, text=True)


def repo_root():
    r = sh(["git", "rev-parse", "--show-toplevel"])
    return r.stdout.strip() if r.returncode == 0 else "."


def latest_release_tag():
    r = sh(["git", "tag"])
    if r.returncode != 0:
        return None
    tags = [t for t in r.stdout.split() if RELEASE_TAG_RE.match(t)]
    if not tags:
        return None
    # Sort by numeric version components.
    tags.sort(key=lambda t: [int(x) for x in t.split(".")])
    return tags[-1]


def parse_schema(text):
    """Return (file_id, {scope_name: {ordinal: member_name}}). Brace-tracked so
    ordinals are attributed to their enclosing interface/struct."""
    file_id = None
    scopes = {}
    stack = []  # (scope_name,) tracked by brace depth
    depth = 0
    for raw in text.splitlines():
        line = raw.split("#", 1)[0]  # strip capnp comments
        if file_id is None:
            m = FILE_ID_RE.match(line)
            if m:
                file_id = m.group(1).lower()
        m = SCOPE_RE.match(line)
        if m:
            # Scope opens on this line (its "{" may be same or next line).
            pending_scope = m.group(1)
        else:
            pending_scope = None
        opens = line.count("{")
        closes = line.count("}")
        if pending_scope and opens:
            stack.append((depth, pending_scope))
            scopes.setdefault(pending_scope, {})
        elif pending_scope:
            # "{" on a later line; remember to push when we see it.
            stack.append((depth, pending_scope))
            scopes.setdefault(pending_scope, {})
        depth += opens - closes
        # Attribute member ordinals to the innermost open scope.
        mm = MEMBER_RE.match(line)
        if mm and stack:
            name, ordinal = mm.group(1), int(mm.group(2))
            scope = stack[-1][1]
            scopes[scope][ordinal] = name
        # Pop scopes whose brace depth we've fallen back below.
        while stack and depth <= stack[-1][0]:
            stack.pop()
    return file_id, scopes


def main():
    os.chdir(repo_root())
    tag = latest_release_tag()
    if not tag:
        print("lint-capnp-schema-compat: no mainnet release tag found; nothing to compare.")
        return 0

    files = sh(["git", "ls-files", f"{CAPNP_DIR}/*.capnp"]).stdout.split()
    problems = []
    checked = new = 0
    for path in files:
        base = sh(["git", "show", f"{tag}:{path}"])
        if base.returncode != 0:
            new += 1
            continue  # new schema since the baseline release
        checked += 1
        with open(path, encoding="utf-8") as fh:
            head_text = fh.read()
        base_id, base_scopes = parse_schema(base.stdout)
        head_id, head_scopes = parse_schema(head_text)
        if base_id and head_id and base_id != head_id:
            problems.append(f"{path}: file id changed {base_id} -> {head_id} (breaks all compatibility)")
        for scope, members in base_scopes.items():
            head_members = head_scopes.get(scope, {})
            for ordinal, name in members.items():
                if ordinal not in head_members:
                    problems.append(f"{path}: {scope} @{ordinal} ('{name}') removed since {tag} "
                                    "(removing/renumbering a released ordinal breaks the wire)")
                elif head_members[ordinal] != name:
                    problems.append(
                        f"{path}: {scope} @{ordinal} renamed '{name}' -> '{head_members[ordinal]}' since {tag} "
                        "(wire-safe, but breaks the generated proxy API and $Proxy.name mapping)")

    if problems:
        print(f"Cap'n Proto schema compatibility regressions vs release {tag}:")
        for p in problems:
            print(f"  {p}")
        print("\nAppend new ordinals; never renumber, reuse, or remove a released one.")
        return 1

    print(f"lint-capnp-schema-compat: OK ({checked} schema(s) checked vs {tag}, {new} new).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
