#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# A thread_local variable must be trivially destructible.
#
# MinGW implements thread_local with emulated TLS, which frees a thread's storage
# when the pthread exits; the C runtime runs thread_local destructors after that,
# from the thread-exit TLS callback. A destructor therefore runs on freed memory:
# a std::string destructor that reads a stale pointer and frees it corrupted the
# Windows heap (0xc0000374 at start-up and shutdown). Use a fixed-size array, a
# fundamental type or a pointer instead.
#
# This is a textual check: it accepts a declaration only when its type is a
# fundamental type or a pointer (an array of either is fine), only one variable
# is declared per statement and per line, and the declaration ends on the line it
# starts on. Comments are ignored. String literals are not parsed, so a ';' or ','
# inside one can still mislead the declarator count. Subtrees are excluded; libmultiprocess's g_thread_context is tracked
# separately.

export LC_ALL=C

EXCLUDE="^src/(crypto/ctaes|leveldb|secp256k1|univalue|bdb53|ipc/libmultiprocess|minisketch|crc32c)/"
TRIVIAL='(bool|char|wchar_t|char8_t|char16_t|char32_t|short|int|long|float|double|size_t|ssize_t|std::size_t|u?int(8|16|32|64)_t|std::u?int(8|16|32|64)_t|unsigned|signed)'
# A run of fundamental type words, ending at a declarator boundary (so "int" does not match "interface").
FUNDAMENTAL_RE="^${TRIVIAL}([[:space:]]+${TRIVIAL})*([[:space:]]|\\*|$)"
# Any type followed by a pointer declarator.
POINTER_RE='^[A-Za-z_:<>0-9,[:space:]]*\*[[:space:]]*(const[[:space:]]+)?[A-Za-z_]'

# True when the declaration (up to its ';') may have more than one declarator: a
# comma outside any (), [] or {} nesting. Such a line is rejected outright, so that a
# pointer declarator cannot carry a non-pointer one past the type check. Angle
# brackets do not count as nesting, because in an initializer they may be relational
# operators that would hide a later declarator; the cost is that a pointer to a
# multi-argument template type needs a type alias.
multiple_declarators() {
    local decl=${1%%;*} depth=0 i c
    for (( i = 0; i < ${#decl}; i++ )); do
        c=${decl:i:1}
        case "$c" in
            '('|'['|'{') depth=$((depth + 1)) ;;
            ')'|']'|'}') depth=$((depth - 1)) ;;
            ',') [ "$depth" -eq 0 ] && return 0 ;;
        esac
    done
    return 1
}

EXIT_CODE=0
while IFS= read -r hit; do
    file=${hit%%:*}
    [[ "$file" =~ $EXCLUDE ]] && continue
    code=${hit#*:*:}
    code=${code%%//*}                       # drop a trailing line comment
    [[ "$code" =~ (^|[^A-Za-z0-9_])thread_local([^A-Za-z0-9_]|$) ]] || continue
    # The declared type: what follows thread_local, minus storage and cv keywords.
    type=$(sed -E 's/.*thread_local[[:space:]]+//; s/\b(static|extern|inline|const|constexpr|volatile|mutable)\b[[:space:]]*//g' <<<"$code")
    # Only the last thread_local on a line would be type-checked below; allow one.
    rest=${code#*thread_local}
    if [[ "$rest" =~ (^|[^A-Za-z0-9_])thread_local([^A-Za-z0-9_]|$) ]]; then
        echo "Declare one thread_local per line: ${hit}"
        EXIT_CODE=1
        continue
    fi
    # git grep supplies one physical line, so a declaration continued on the next line
    # (for instance ", value;") would never be examined. Require it to end here.
    trimmed=${code%"${code##*[![:space:]]}"}
    if [[ "$trimmed" != *";" ]]; then
        echo "Keep each thread_local declaration on one line: ${hit}"
        EXIT_CODE=1
        continue
    fi
    if multiple_declarators "$type"; then
        echo "Declare one thread_local per statement: ${hit}"
        EXIT_CODE=1
        continue
    fi
    if [[ "$type" =~ $FUNDAMENTAL_RE ]] || [[ "$type" =~ $POINTER_RE ]]; then
        continue
    fi
    echo "A thread_local here may not be trivially destructible: ${hit}"
    EXIT_CODE=1
done < <(git grep -nE '(^|[^A-Za-z0-9_])thread_local([^A-Za-z0-9_]|$)' -- '*.cpp' '*.h' | grep -vE '^[^:]+:[0-9]+:[[:space:]]*(//|\*|/\*)')

if [ "$EXIT_CODE" != 0 ]; then
    echo
    echo "thread_local must be trivially destructible: MinGW's emulated TLS frees the"
    echo "storage before the C runtime runs the destructor (see test/lint/lint-thread-local.sh)."
fi
exit "$EXIT_CODE"
