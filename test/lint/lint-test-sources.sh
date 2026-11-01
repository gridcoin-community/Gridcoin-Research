#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Every .cpp under src/test/ must be wired into src/test/CMakeLists.txt.
#
# A file in the tree that no CMakeLists names is never compiled, never linked
# into test_gridcoin and never run, while looking exactly like coverage to
# anyone reading it. key_io_tests.cpp was wired into the autotools build and
# never into CMake, so it dropped out silently when autotools was removed and
# nothing noticed for months (issue #3355). A build-system migration is exactly
# when this happens, and exactly when nobody is looking for it.
#
# What counts as wiring is deliberately loose, because sources reach the target
# by several routes: the add_executable list, target_sources() inside a
# conditional block (ipc_wire_handshake_tests.cpp), and set_property(SOURCE ...)
# (test_gridcoin.cpp). A deliberate omission is recorded by commenting the entry
# out, as compilerbug_tests.cpp does, and that counts too -- the check is that
# the file was considered, not how.
#
# What does NOT count is a filename mentioned inside an explanatory comment.
# src/test/CMakeLists.txt is comment-heavy and its prose names source files, so
# accepting any occurrence would leave the check green for a file whose real
# wiring had been deleted -- true today of both ipc_wire_handshake_tests.cpp and
# alt_signal_stack.cpp, each named once in wiring and once in a paragraph.

export LC_ALL=C

# Paths below are repository-relative, so anchor there rather than requiring the
# caller to be in the right directory (lint-qt-includes.sh does the same). The
# toplevel is captured and tested separately: `cd "$(git ...)"` is not enough,
# because a failed rev-parse yields an empty string and `cd ""` is a successful
# no-op, which would leave this check silently passing over no files at all.
TOPLEVEL=$(git rev-parse --show-toplevel 2>/dev/null)
if [ -z "${TOPLEVEL}" ] || ! cd "${TOPLEVEL}"; then
    echo "lint-test-sources: not inside a git repository."
    exit 1
fi

CMAKELISTS="src/test/CMakeLists.txt"

if [ ! -f "${CMAKELISTS}" ]; then
    echo "lint-test-sources: ${CMAKELISTS} is missing."
    exit 1
fi

# Reduce the CMakeLists to wiring lines: every non-comment line as-is, plus the
# commented-out-entry convention -- a comment whose entire content is one source
# path. Prose comments drop out here, which is the whole point.
WIRING=$(awk '
    {
        stripped = $0
        sub(/^[[:space:]]+/, "", stripped)
        if (stripped ~ /^#/) {
            sub(/^#[[:space:]]*/, "", stripped)
            if (stripped ~ /^[A-Za-z0-9_\/.+-]+\.(cpp|c)$/) print stripped
        } else {
            print $0
        }
    }
' "${CMAKELISTS}")

EXIT_CODE=0
FOUND_ANY=0

for SOURCE_FILE in $(git ls-files -- "src/test/**.cpp"); do
    FOUND_ANY=1
    REL=${SOURCE_FILE#src/test/}
    # Word-boundary match so foo_tests.cpp cannot be satisfied by bar_foo_tests.cpp.
    if ! printf '%s\n' "${WIRING}" | grep -qE "(^|[^[:alnum:]_/.-])${REL//./\\.}([^[:alnum:]_]|$)"; then
        echo "${SOURCE_FILE} is not wired into ${CMAKELISTS}, so it is never built or run."
        echo "  Add it to the test_gridcoin sources, or comment the entry out to record that the omission is deliberate."
        EXIT_CODE=1
    fi
done

# Guard against the pathspec silently matching nothing (a wrong glob, or a
# git version that treats the pattern differently) and the check passing on air.
if [ "${FOUND_ANY}" -eq 0 ]; then
    echo "lint-test-sources: found no sources under src/test/; the pathspec is wrong."
    exit 1
fi

exit ${EXIT_CODE}
