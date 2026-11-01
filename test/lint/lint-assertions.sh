#!/usr/bin/env bash
#
# Copyright (c) 2018-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Check for assertions with obvious side effects.

export LC_ALL=C

EXIT_CODE=0

# PRE31-C (SEI CERT C Coding Standard):
# "Assertions should not contain assignments, increment, or decrement operators."
OUTPUT=$(git grep -E '[^_]assert\(.*(\+\+|\-\-|[^=!<>]=[^=!<>]).*\);' -- "*.cpp" "*.h")
if [[ ${OUTPUT} != "" ]]; then
    echo "Assertions should not have side effects:"
    echo
    echo "${OUTPUT}"
    EXIT_CODE=1
fi

# Macro CHECK_NONFATAL(condition) should be used instead of assert for RPC code, where it
# is undesirable to crash the whole program. See: src/util/check.h
# src/rpc/server.cpp is excluded from this check since it's mostly meta-code.
OUTPUT=$(git grep -nE '\<(A|a)ssert *\(.*\);' -- "src/rpc/" "src/wallet/rpc*" ":(exclude)src/rpc/server.cpp")
if [[ ${OUTPUT} != "" ]]; then
    echo "CHECK_NONFATAL(condition) should be used instead of assert for RPC code."
    echo
    echo "${OUTPUT}"
    EXIT_CODE=1
fi

# A bare assert() ADDED to first-party code is a crash primitive: on a peer-facing
# path it hands a remote party an abort, and everywhere else it turns a recoverable
# state into a stopped node. This rule is diff-scoped rather than tree-wide -- the
# tree carries several hundred inherited asserts and a tree-wide rule would only ever
# be silenced -- so it asks one thing of new code: justify it or use something that
# does not abort.
#
# Scope is a deny-list, not a named path set: everything first-party is covered, so a
# new peer-facing file is covered the day it is added. That is deliberate. Every
# previous sweep of this tree enumerated peer-facing files BY NAME and every one of
# them omitted src/script/, the largest peer-data-driven evaluator here.
#
# Deliberate ones carry "LINT-OK-ASSERT: <reason>" ON THE SAME LINE -- the check reads the
# diff line by line, so a comment on the line above would not be seen. That trailing reason
# is the review record for why aborting is the right answer at that site.
#
# A pre-existing assert MOVED between files inside the range reads as added and is
# flagged. Accepted: the strict direction, and a move of an assert is a fine moment
# to re-justify it (or convert it -- see the guidance below).
#
# COMMIT_RANGE follows lint-whitespace.sh: unset means HEAD, i.e. the working tree.
# The quality workflow exports it as base..head for the whole lint step, so on a pull
# request this sees exactly the commits under review. On a plain push that expression
# expands to a bare ".." -- say so and check nothing, rather than passing silently on a
# range git cannot parse.
if [ -z "${COMMIT_RANGE:-}" ]; then
    ASSERT_COMMIT_RANGE="HEAD"
else
    ASSERT_COMMIT_RANGE="${COMMIT_RANGE}"
fi

# Both endpoints must resolve: git diff errors are suppressed below, so an
# unresolvable side would otherwise yield an empty diff and a silent pass --
# the exact vacuous-pass failure this rule exists to avoid.
if [ "${ASSERT_COMMIT_RANGE}" = ".." ] \
    || ! git rev-parse --quiet --verify "${ASSERT_COMMIT_RANGE%%..*}" > /dev/null 2>&1 \
    || ! git rev-parse --quiet --verify "${ASSERT_COMMIT_RANGE##*..}" > /dev/null 2>&1; then
    echo "lint-assertions: no usable COMMIT_RANGE (${ASSERT_COMMIT_RANGE}); skipping the added-assert check."
    exit ${EXIT_CODE}
fi

VENDORED_EXCLUDES=(
    ":(exclude)src/bdb53/*"
    ":(exclude)src/leveldb/*"
    ":(exclude)src/univalue/*"
    ":(exclude)src/secp256k1/*"
    ":(exclude)src/crc32c/*"
    ":(exclude)src/crypto/ctaes/*"
    ":(exclude)src/ipc/libmultiprocess/*"
)

# Both endpoints resolved above, so a diff failure here is environmental --
# fail loudly rather than passing on an empty diff.
if ! DIFF_ADDED=$(git diff -U0 "${ASSERT_COMMIT_RANGE}" -- "*.cpp" "*.h" "${VENDORED_EXCLUDES[@]}" 2>/dev/null); then
    echo "lint-assertions: git diff failed for ${ASSERT_COMMIT_RANGE}; refusing to pass vacuously."
    exit 1
fi
# Marker-carrying lines are excused first: the marker lives in a comment, so it
# must be honored before comment text is stripped. Then // tails, complete
# /* */ spans, unterminated /* openings, and block-comment continuation lines
# (leading *) are dropped, so assert( in prose -- a comment discussing an
# assert, commented-out code -- does not read as a call site. static_assert is
# neutralized by token substitution rather than dropping the line, so a bare
# assert sharing a line with one still fails.
ADDED_CANDIDATES=$(printf '%s\n' "${DIFF_ADDED}" | grep -E "^\+" | grep -vE "^\+\+\+" | grep -vE "LINT-OK-ASSERT" | grep -vE "^\+[[:space:]]*\*([[:space:]/]|$)")
ADDED_ASSERTS=$(printf '%s\n' "${ADDED_CANDIDATES}" | sed -E -e 's|//.*$||' -e 's@/\*([^*]|\*+[^/*])*\*+/@@g' -e 's@/\*.*$@@' -e 's|static_assert|STATIC_ASSERT_|g' | grep -E "(^|[^_[:alnum:]])assert[[:space:]]*\(")

if [[ ${ADDED_ASSERTS} != "" ]]; then
    echo "New bare assert() in first-party code. On a peer-facing path an assert is a remote"
    echo "abort; elsewhere it stops a node that could have carried on. Prefer, in order:"
    echo "  - returning an error: peer-reachable code rejects and carries on, never aborts;"
    echo "  - CHECK_NONFATAL for RPC code (throws; surfaced to the caller);"
    echo "  - Assert() from util/check.h for an invariant that genuinely must halt: it"
    echo "    always evaluates and halts even under -DNDEBUG, unlike bare assert;"
    echo "  - Assume() from util/check.h for a should-hold check tolerated in production."
    echo "If a bare assert really is right, say why with a trailing"
    echo "LINT-OK-ASSERT: <reason> comment on the same line."
    echo
    echo "${ADDED_ASSERTS}"
    EXIT_CODE=1
fi

exit ${EXIT_CODE}
