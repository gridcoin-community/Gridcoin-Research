#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Checks on the QML front end's two trees.
#
# src/qt/quick, the C++ module, must never block the GUI thread: every prompt
# is a request, then a signal, then an answer, and long calls run on
# QmlCallRunner's lanes. The first check fails on the constructs that block or
# spin a nested event loop: QEventLoop, an exec() call, a blocking queued
# connection (including GUIUtil's helper that returns one) and
# waitForFinished. The module's tests (src/qt/quick/test/) are not scanned: a
# test may run the application's own event loop, as the quit test does.
#
# src/qt/qml, the carried QML front end, holds QML only, so the module's checks
# cannot be sidestepped there. Its files still bind the context properties of
# the branch they came from; only the files listed below may, and a change
# that binds a file to Gridcoin.Qml removes it from the list.
#
# Neither tree may name Apple's SF Pro fonts: the QML front end takes its font
# family from Fonts.uiFamily (the platform's system font on Apple platforms,
# the bundled Inter elsewhere), and SF Pro's licence does not allow bundling
# it, so no file under src/qt may be one.

export LC_ALL=C

cd "$(git rev-parse --show-toplevel)" || exit 1

EXIT_CODE=0

if [ -z "$(git ls-files -- src/qt/quick ':!src/qt/quick/test')" ]; then
    echo "lint-qml-module: found no files under src/qt/quick; the pathspec is wrong."
    exit 1
fi
if [ -z "$(git ls-files -- src/qt/qml)" ]; then
    echo "lint-qml-module: found no files under src/qt/qml; the pathspec is wrong."
    exit 1
fi

PATTERN='QEventLoop|\bexec\(\)|BlockingQueuedConnection|blockingGUIThreadConnection|waitForFinished'
OUTPUT=$(git grep -n -E "${PATTERN}" -- src/qt/quick ':!src/qt/quick/test')
RC=$? # 0: a match, 1: no match, anything else: git grep failed
if [ ${RC} -eq 0 ]; then
    echo "Blocking construct(s) in the QML module (src/qt/quick, tests excluded):"
    echo "${OUTPUT}"
    EXIT_CODE=1
elif [ ${RC} -ne 1 ]; then
    echo "lint-qml-module: git grep failed (exit ${RC})."
    exit 1
fi

OUTPUT=$(git ls-files -- 'src/qt/qml/*.cpp' 'src/qt/qml/*.h')
if [ -n "${OUTPUT}" ]; then
    echo "C++ under the carried QML tree (src/qt/qml holds QML only):"
    echo "${OUTPUT}"
    EXIT_CODE=1
fi

LEGACY_PATTERN='_(initModel|clientModel|walletModel|researcherModel|mrcModel|votingModel|sendCoinsController|nativeDialog)\b'
LEGACY_ALLOWED=(
    src/qt/qml/AboutWindow.qml
    src/qt/qml/EditAddressDialog.qml
    src/qt/qml/MainWindow.qml
    src/qt/qml/OverviewView.qml
    src/qt/qml/ReceiveView.qml
    src/qt/qml/SendView.qml
    src/qt/qml/SplashScreen.qml
    src/qt/qml/StatusFooter.qml
    src/qt/qml/TabMenu.qml
    src/qt/qml/UnlockDialog.qml
    src/qt/qml/WindowManager.qml
)
OUTPUT=$(git grep -l -E "${LEGACY_PATTERN}" -- src/qt/qml)
RC=$? # 0: a match, 1: no match, anything else: git grep failed
if [ ${RC} -gt 1 ]; then
    echo "lint-qml-module: git grep failed (exit ${RC})."
    exit 1
fi
while IFS= read -r FILE; do
    [ -n "${FILE}" ] || continue
    LISTED=0
    for ALLOWED in "${LEGACY_ALLOWED[@]}"; do
        if [ "${FILE}" = "${ALLOWED}" ]; then
            LISTED=1
            break
        fi
    done
    if [ ${LISTED} -eq 0 ]; then
        echo "${FILE} uses a context property of the branch the QML came from; bind it to Gridcoin.Qml instead:"
        git grep -n -E "${LEGACY_PATTERN}" -- "${FILE}"
        EXIT_CODE=1
    fi
done <<< "${OUTPUT}"

OUTPUT=$(git grep -n -i -E 'SF[ -]Pro' -- src/qt/qml src/qt/quick)
RC=$? # 0: a match, 1: no match, anything else: git grep failed
if [ ${RC} -eq 0 ]; then
    echo "SF Pro named under src/qt/qml or src/qt/quick (its licence does not allow embedding it):"
    echo "${OUTPUT}"
    EXIT_CODE=1
elif [ ${RC} -ne 1 ]; then
    echo "lint-qml-module: git grep failed (exit ${RC})."
    exit 1
fi
OUTPUT=$(git ls-files -- 'src/qt/*SF-Pro*')
if [ -n "${OUTPUT}" ]; then
    echo "SF Pro font file(s) under src/qt:"
    echo "${OUTPUT}"
    EXIT_CODE=1
fi

exit ${EXIT_CODE}
