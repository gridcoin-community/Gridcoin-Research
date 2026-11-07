#!/usr/bin/env python3
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
"""Fail on an exception's what() printed from a Windows-only source without AsciiWhat.

std::system_error's what() carries the system's message, which Windows words in its display
language, and a log line, a console or an error text carries ASCII only. In the Windows-only
sources every .what() call must be the argument of AsciiWhat( (util/syserror.h), on the same line,
unless that line or the one above carries "// what() is ASCII:" and a reason (for a catch that
only sees an exception of our own with an ASCII message).
"""

import glob
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')

PATTERNS = [
    'src/winsvc/*.cpp',
    'src/ipc/win_*.cpp',
    'src/util/win*.cpp',
    'src/wallet/autounlock_dpapi.cpp',
]

WHAT = re.compile(r'(?:\.|->)what\s*\(\s*\)')
WRAPPED = re.compile(r'AsciiWhat\s*\(\s*[A-Za-z_][A-Za-z0-9_]*\s*\)')
EXEMPT = '// what() is ASCII:'


def violations(path, lines):
    found = []
    for number, line in enumerate(lines, 1):
        code = line.split('//')[0] if EXEMPT not in line else line
        if not WHAT.search(code):
            continue
        if WRAPPED.search(line) and not WHAT.search(WRAPPED.sub('', code)):
            continue
        if EXEMPT in line or (number > 1 and EXEMPT in lines[number - 2]):
            continue
        found.append('%s:%d: what() is printed without AsciiWhat(): %s' % (path, number, line.strip()))
    return found


def self_test():
    good = [
        'LogPrintf("x: %s", AsciiWhat(e));',
        'error = AsciiWhat(e);',
        '// the what() of a system_error is not ASCII',
        'Say("a") + std::string(AsciiWhat(e));',
    ]
    bad = [
        'LogPrintf("x: %s", e.what());',
        'error = e.what();',
        'x = AsciiWhat(e) + other.what();',
        'x = p->what();',
    ]
    exempt = ['// what() is ASCII: only our own runtime_error arrives here', 'error = e.what();']
    ok = True
    if violations('good', good):
        print('self-test: a good sample was flagged')
        ok = False
    for sample in bad:
        if not violations('bad', [sample]):
            print('self-test: a bad sample was not flagged: ' + sample)
            ok = False
    if violations('exempt', exempt):
        print('self-test: an exempt sample was flagged')
        ok = False
    if violations('exempt', [exempt[1]]) == []:
        print('self-test: an exemption was honoured without its comment')
        ok = False
    return ok


def main():
    if '--self-test' in sys.argv:
        return 0 if self_test() else 1

    problems = []
    checked = 0
    for pattern in PATTERNS:
        for path in sorted(glob.glob(os.path.join(ROOT, pattern))):
            checked += 1
            with open(path, encoding='utf-8') as f:
                problems += violations(os.path.relpath(path, ROOT), f.read().split('\n'))
    if checked == 0:
        print('lint-ascii-what: no source file found')
        return 1
    for problem in problems:
        print(problem)
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
