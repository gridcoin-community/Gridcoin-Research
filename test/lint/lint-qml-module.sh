#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Checks on the QML front end's two trees.
#
# src/qt/quick, the C++ module, does not hold the GUI thread for a prompt or a
# long call: every prompt is a request, then a signal, then an answer, and long
# calls run on QmlCallRunner's lanes. Two waits are by design:
# QmlCallRunner::drain(), where teardown waits for an in-flight serial call,
# and qml_main.cpp's start-up error box, shown before any event loop runs
# (qml_main.cpp is not scanned). The first check is a tripwire for the common
# constructs that block or spin a nested event loop, not a proof that none
# exists: QEventLoop, an exec call, a blocking queued connection (including
# GUIUtil's helper that returns one), waitForFinished, waitForDone,
# processEvents, qWait, a QFuture result, and the static dialogs of
# QMessageBox, QFileDialog, QInputDialog, QColorDialog and QFontDialog. The
# module's tests (src/qt/quick/test/) are not scanned: a test may run the
# application's own event loop, as the quit test does.
#
# src/qt/qml, the carried QML front end, holds QML only, so the module's checks
# cannot be sidestepped there. Its files still bind the context properties of
# the branch they came from; only the files listed below may, and a change
# that binds a file to Gridcoin.Qml removes it from the list. A listed file
# that binds none of them fails too, so the list never outgrows the tree.
#
# The root, src/qt/qml/WindowManager.qml, restores its main window only from
# the tray: it binds no Qt.application state. It changes a window's state only
# through the QWindow methods, never by assigning visible or visibility: Qt 6.7
# and later defer that assignment for a window whose parent is the root Item.
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

PATTERN='QEventLoop|\bexec[[:space:]]*\(|BlockingQueuedConnection|blockingGUIThreadConnection|waitForFinished|waitForDone|processEvents|qWait|\.result[[:space:]]*\(|QMessageBox::(critical|warning|information|question|about)|QFileDialog::get|QInputDialog::get|QColorDialog::getColor|QFontDialog::getFont'
OUTPUT=$(git grep -n -E "${PATTERN}" -- src/qt/quick ':!src/qt/quick/test' ':!src/qt/quick/qml_main.cpp')
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
    src/qt/qml/EditAddressDialog.qml
    src/qt/qml/ReceiveView.qml
    src/qt/qml/SendView.qml
    src/qt/qml/UnlockDialog.qml
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
for ALLOWED in "${LEGACY_ALLOWED[@]}"; do
    if ! grep -q -x -F -- "${ALLOWED}" <<< "${OUTPUT}"; then
        echo "${ALLOWED} is listed but binds no legacy name; remove it from LEGACY_ALLOWED."
        EXIT_CODE=1
    fi
done

OUTPUT=$(git grep -n 'Qt\.application' -- src/qt/qml/WindowManager.qml)
RC=$? # 0: a match, 1: no match, anything else: git grep failed
if [ ${RC} -eq 0 ]; then
    echo "WindowManager.qml binds Qt.application; the root restores its main window from the tray only:"
    echo "${OUTPUT}"
    EXIT_CODE=1
elif [ ${RC} -ne 1 ]; then
    echo "lint-qml-module: git grep failed (exit ${RC})."
    exit 1
fi

OUTPUT=$(git grep -n -E '\.(visible|visibility)[[:space:]]*=([^=]|$)' -- src/qt/qml/WindowManager.qml)
RC=$? # 0: a match, 1: no match, anything else: git grep failed
if [ ${RC} -eq 0 ]; then
    echo "WindowManager.qml assigns visible or visibility on a window; Qt 6.7+ defers that assignment for a window parented to the root Item (doc/qml-module-api.md section 8), so call show(), showNormal(), showMinimized(), showMaximized(), setVisible() or hide() instead:"
    echo "${OUTPUT}"
    EXIT_CODE=1
elif [ ${RC} -ne 1 ]; then
    echo "lint-qml-module: git grep failed (exit ${RC})."
    exit 1
fi

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
