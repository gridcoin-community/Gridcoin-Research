#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# The QML front end's C++ module (src/qt/quick) must never block the GUI
# thread: every prompt is a request, then a signal, then an answer, and long
# calls run on QmlCallRunner's lanes. This check fails on the constructs that
# block or spin a nested event loop: QEventLoop, an exec() call, a blocking
# queued connection (including GUIUtil's helper that returns one) and
# waitForFinished.
#
# The module's tests (src/qt/quick/test/) are not scanned: a test may run the
# application's own event loop, as the quit test does.

export LC_ALL=C

cd "$(git rev-parse --show-toplevel)" || exit 1

if [ -z "$(git ls-files -- src/qt/quick ':!src/qt/quick/test')" ]; then
    echo "lint-qml-module: found no files under src/qt/quick; the pathspec is wrong."
    exit 1
fi

PATTERN='QEventLoop|\bexec\(\)|BlockingQueuedConnection|blockingGUIThreadConnection|waitForFinished'

OUTPUT=$(git grep -n -E "${PATTERN}" -- src/qt/quick ':!src/qt/quick/test')
RC=$? # 0: a match, 1: no match, anything else: git grep failed
if [ ${RC} -eq 0 ]; then
    echo "Blocking construct(s) in the QML module (src/qt/quick, tests excluded):"
    echo "${OUTPUT}"
    exit 1
elif [ ${RC} -ne 1 ]; then
    echo "lint-qml-module: git grep failed (exit ${RC})."
    exit 1
fi

exit 0
