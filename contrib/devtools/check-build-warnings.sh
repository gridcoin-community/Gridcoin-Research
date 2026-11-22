#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Fail a build that prints a compiler or linker warning.
#
# Usage: check-build-warnings.sh -- <command> [<arg>...]
#        check-build-warnings.sh --log <file>
#
# The first form runs the command, passes its output through as it goes, and
# then scans that output. The second scans a log that already exists, such as a
# job log downloaded from GitHub (the timestamp GitHub puts on every line is
# ignored).
#
# Exit status: the command's own when it failed, with any warnings still listed
# for the diagnosis; otherwise 1 when there is a warning and 0 when there is
# none. A usage error, or output that could not be saved or scanned, exits 2.
#
# What this guards is the tree's OWN build. CI wraps only the commands that
# build the tree -- which takes in the subtrees under src/ and the links against
# the bundled Berkeley DB -- and never depends, the Flatpak manifest's dependency
# modules or the macOS deploy step, whose warnings come from code this project
# does not compile. Every CI job built the tree without one when this was added,
# so there is no baseline to compare against: any warning is a new one.
#
# It sees only what the run compiles. An incremental build that finds an object
# up to date prints nothing for it, so a warning in that file goes unseen until
# the file is compiled again; CI builds from scratch.
#
# This is not -DWERROR=ON. That promotes ten chosen -W flags and stops the build
# at the first; it never sees a linker, a driver, or a warning outside its list.
#
# A warning, for this script, is one of these lines (English diagnostics; the CI
# runners and containers use the C or en_US locale):
#   file:line[:col]: warning: ...          GCC, Clang and moc
#   <command-line>: warning: ...           GCC: a macro redefined on the command line
#   <command line>:1:9: warning: ...       Clang: the same
#   cc1plus: warning: ...                  a compiler driver or pass, cross prefixes
#   x86_64-w64-mingw32-g++-posix: warning: and version suffixes included
#   warning: ... [-Wflag]                  Clang: a diagnostic about its own options
#   ld: warning: ...                       Apple ld; GNU ld, gold and lld, including
#   /usr/bin/ld: foo.o: warning: ...       the cross and mingw ones
#   foo.c:(.text+0x1a): warning: ...       GNU ld, on a use inside a function
#   foo.s:12: Warning: ...                 the GNU assembler, and Qt 5's moc
#   {standard input}:12: Warning: ...      the assembler on the compiler's output
#   foo.s: Warning: ...                    the assembler with no line number, as for
#   {standard input}: Warning: ...         a file that ends inside a line
#   .../ranlib: file: x.a(y.o) has no symbols
#                                          Apple ranlib and libtool
#   warning: .../ranlib: ...               the same, on an archive
# The build tools' own status messages (make, ninja, CMake) are not counted, which
# is why the assembler's line-less form is matched only on an assembler file name:
# make says "make: Warning: File ... has modification time ... in the future". A
# make warning that names a line of a makefile (Makefile:12: warning: ...) is
# counted, being a defect in the build files.

# This script is intentionally locale dependent by not setting "export LC_ALL=C":
# the build it wraps runs in the caller's locale. The scan sets LC_ALL=C itself.

set -uo pipefail

usage() {
    echo "usage: $0 -- <command> [<arg>...]" >&2
    echo "       $0 --log <file>" >&2
}

WARNING_PATTERNS=(
    '^[^[:space:]](.*[^[:space:]])?:[0-9]+(:[0-9]+)?: warning: '
    '^<(command[- ]line|built-in)>(:[0-9]+)*: warning: '
    '^([^[:space:]].*/)?([[:alnum:]_.+-]+-)?(cc1|cc1plus|cc1obj|cc1objplus|collect2|lto1|lto-wrapper|cc|gcc|c[+][+]|g[+][+]|clang|clang[+][+])(-[0-9.]+)?(-posix|-win32)?(\.exe)?: warning: '
    '^warning: .*[[]-W[[:alnum:]_=+-]+[]]$'
    '(^|[[:space:]/])([[:alnum:]_.+-]+-)?(ld|ld64)(\.[[:alnum:]]+)?(\.exe)?: (.*: )?warning: '
    '^[^[:space:]](.*[^[:space:]])?:([0-9]+:)?[(][^)]*[)]: warning: '
    '^[^[:space:]](.*[^[:space:]])?:[0-9]+: Warning: '
    '^([^[:space:]](.*[^[:space:]])?[.](s|S|sx)|[{]standard input[}]): Warning: '
    '(^|/)(ranlib|libtool): file: .* has no symbols$'
    '^warning: (.*/)?(ranlib|libtool): '
)

LOG=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        --log)
            [ "$#" -ge 2 ] || { usage; exit 2; }
            LOG="$2"
            shift 2
            ;;
        --)
            shift
            break
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage
            exit 2
            ;;
    esac
done

# Exactly one of --log and a command.
if [ -n "$LOG" ]; then
    [ "$#" -eq 0 ] || { usage; exit 2; }
    if [ ! -r "$LOG" ]; then
        echo "check-build-warnings: cannot read ${LOG}" >&2
        exit 2
    fi
    STATUS=0
else
    [ "$#" -gt 0 ] || { usage; exit 2; }
    # Explicit template: BSD/macOS mktemp requires one. Respects $TMPDIR.
    LOG=$(mktemp "${TMPDIR:-/tmp}/check-build-warnings.XXXXXX") || exit 2
    trap 'rm -f "$LOG"' EXIT
    "$@" 2>&1 | tee "$LOG"
    PIPE=("${PIPESTATUS[@]}")
    STATUS=${PIPE[0]}
    # tee still passes the output through when it cannot write the log (a full
    # disk), so the build can finish while the scan would read a truncated copy.
    if [ "${PIPE[1]}" -ne 0 ]; then
        echo "check-build-warnings: could not save the build output to ${LOG}; it was not scanned" >&2
        [ -z "${GITHUB_ACTIONS:-}" ] || echo "::error title=Build warnings::the build output could not be saved, so it was not scanned"
        if [ "$STATUS" -ne 0 ]; then
            exit "$STATUS"
        fi
        exit 2
    fi
fi

GREP_ARGS=()
for pattern in "${WARNING_PATTERNS[@]}"; do
    GREP_ARGS+=(-e "$pattern")
done
ESC=$(printf '\033')
CR=$(printf '\r')

# Drop NUL bytes (grep would call the log binary and print none of its lines),
# GitHub's timestamps, colour codes and carriage returns, and print the warning
# lines. LC_ALL=C here only: set for the whole script it would also change the
# language of the wrapped build.
scan() {
    LC_ALL=C tr -d '\000' < "$LOG" |
        LC_ALL=C sed -E -e 's/^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:.]+Z //' \
            -e "s/${ESC}\[[0-9;]*[A-Za-z]//g" -e "s/${CR}\$//" |
        LC_ALL=C grep -E "${GREP_ARGS[@]}"
    local status=("${PIPESTATUS[@]}")
    # grep exits 1 when no line matched, which is a clean scan. Any other
    # failure means the log was not read through, so nothing can be said.
    if [ "${status[0]}" -ne 0 ] || [ "${status[1]}" -ne 0 ] || [ "${status[2]}" -gt 1 ]; then
        return 2
    fi
}

FOUND=$(scan)
if [ "$?" -eq 2 ]; then
    echo "check-build-warnings: could not scan ${LOG}" >&2
    [ -z "${GITHUB_ACTIONS:-}" ] || echo "::error title=Build warnings::the build output could not be scanned"
    if [ "$STATUS" -ne 0 ]; then
        exit "$STATUS"
    fi
    exit 2
fi

if [ -z "$FOUND" ]; then
    echo "check-build-warnings: no compiler or linker warnings"
    exit "$STATUS"
fi

TOTAL=$(printf '%s\n' "$FOUND" | wc -l | tr -d ' ')
DISTINCT=$(printf '%s\n' "$FOUND" | LC_ALL=C sort | uniq -c | LC_ALL=C sort -rn)
NDISTINCT=$(printf '%s\n' "$DISTINCT" | wc -l | tr -d ' ')
echo
echo "check-build-warnings: ${TOTAL} compiler or linker warning line(s), ${NDISTINCT} distinct (count, line):"
printf '%s\n' "$DISTINCT"
if [ -n "${GITHUB_ACTIONS:-}" ]; then
    echo "::error title=Build warnings::${TOTAL} compiler or linker warnings in the tree's own build" \
        "(${NDISTINCT} distinct); the list is at the end of this step's log"
fi

if [ "$STATUS" -ne 0 ]; then
    exit "$STATUS"
fi
exit 1
