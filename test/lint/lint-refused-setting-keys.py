#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

"""Refused setting keys lint.

share/gridcoin/refused-setting-keys.txt is the one source of the Refuse class
patterns for the core (compiled in) and for tools (installed). This checks that:

  * every line that is not blank or a comment matches ^[a-z0-9_*-]+$,
  * no pattern appears twice,
  * src/CMakeLists.txt installs the file,
  * fnmatch.fnmatchcase and the C++ matcher agree on the grammar, by running the
    vectors of the setting_key_patterns_match_whole_names test in
    src/test/util_tests.cpp through fnmatch.
"""

import fnmatch
import os
import re
import sys

PATTERN_FILE = "share/gridcoin/refused-setting-keys.txt"
CMAKE_FILE = "src/CMakeLists.txt"
GRAMMAR = re.compile(r"^[a-z0-9_*-]+$")

# (pattern, name, expected); the same eleven vectors as the C++ test
# setting_key_patterns_match_whole_names in src/test/util_tests.cpp.
VECTORS = [
    ("*notify", "blocknotify", True),
    ("*notify", "notify", True),
    ("*notify", "notifyx", False),
    ("rpc*", "rpc", True),
    ("clear*history", "clearbeaconhistory", True),
    ("clear*history", "clearbeaconhistoryx", False),
    ("min", "min", True),
    ("min", "mininput", False),
    ("rpc*", "RPCPORT", False),
    ("*", "", True),
    ("a*b*c", "acb", False),
]


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
    os.chdir(root)
    errors = []

    seen = {}
    with open(PATTERN_FILE, encoding="utf-8") as f:
        for number, raw in enumerate(f.read().split("\n"), start=1):
            line = raw.rstrip("\r")
            if line == "" or line.startswith("#"):
                continue
            if not GRAMMAR.match(line):
                errors.append("%s:%d: '%s' is outside the pattern grammar" % (PATTERN_FILE, number, line))
            if line in seen:
                errors.append("%s:%d: duplicate of line %d: '%s'" % (PATTERN_FILE, number, seen[line], line))
            seen.setdefault(line, number)
    if not seen:
        errors.append("%s: no patterns" % PATTERN_FILE)

    with open(CMAKE_FILE, encoding="utf-8") as f:
        cmake = f.read()
    if not re.search(r'install\(FILES\s+"\$\{CMAKE_SOURCE_DIR\}/' + re.escape(PATTERN_FILE) + '"', cmake):
        errors.append("%s does not install %s" % (CMAKE_FILE, PATTERN_FILE))

    for pattern, name, expected in VECTORS:
        if fnmatch.fnmatchcase(name, pattern) != expected:
            errors.append("fnmatch disagrees with the C++ matcher: pattern '%s' name '%s' expected %s"
                          % (pattern, name, expected))

    for e in errors:
        print("lint-refused-setting-keys: " + e, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
