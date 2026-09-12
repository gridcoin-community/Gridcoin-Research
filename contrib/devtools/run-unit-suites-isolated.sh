#!/usr/bin/env bash
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Run every top-level suite of test_gridcoin in its own process, and report
# the suites that fail alone. A suite that passes in the whole-binary run
# but fails here depended on state some earlier suite left behind; one that
# fails in the whole-binary run but passes here was broken by such state.
#
# With --random SEED the whole binary is run once in Boost's shuffled order
# instead, which exposes the same dependency from the other direction.
#
# Usage:
#   contrib/devtools/run-unit-suites-isolated.sh <path/to/test_gridcoin>
#   contrib/devtools/run-unit-suites-isolated.sh --random <seed> <path/to/test_gridcoin>
#
# The binary is run from a temporary directory: it writes banlist.dat and
# similar files into its working directory.

export LC_ALL=C
set -euo pipefail

usage() {
    echo "usage: $0 [--random <seed>] <test_gridcoin>" >&2
    exit 2
}

seed=""
if [ "${1:-}" = "--random" ]; then
    seed="${2:-}"
    [ -n "$seed" ] || usage
    shift 2
fi

[ $# -eq 1 ] || usage
bin="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
[ -x "$bin" ] || { echo "not executable: $bin" >&2; exit 2; }

workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT
cd "$workdir"

if [ -n "$seed" ]; then
    echo "== test_gridcoin --random=$seed"
    "$bin" --random="$seed"
    exit $?
fi

# --list_content prints the tree to stderr: a top-level suite is an unindented
# name ending in '*' (enabled); its cases are indented beneath it.
mapfile -t suites < <("$bin" --list_content 2>&1 | sed -n 's/^\([A-Za-z0-9_]*\)\*$/\1/p')
[ "${#suites[@]}" -gt 0 ] || { echo "no suites listed by $bin" >&2; exit 2; }

failed=()
for suite in "${suites[@]}"; do
    if "$bin" --run_test="$suite" > "$suite.log" 2>&1; then
        echo "ok   $suite"
    else
        echo "FAIL $suite"
        failed+=("$suite")
    fi
done

echo
echo "== ${#suites[@]} suites run alone, ${#failed[@]} failed"

if [ "${#failed[@]}" -gt 0 ]; then
    for suite in "${failed[@]}"; do
        echo
        echo "---- $suite (last 40 lines)"
        tail -n 40 "$suite.log"
    done
    exit 1
fi
