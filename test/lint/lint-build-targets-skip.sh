#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Runs build_targets.sh against stand-ins for cmake, ctest, uname and brew and
# checks which reruns it skips as up to date.
#
# should_skip_build skips a target when the recorded build state equals the
# current one and every file in the target's ARTIFACTS list exists. A build
# output that is not in the list can be deleted without the next run noticing:
# it prints "Build up-to-date" and builds nothing. The scenarios below delete
# one listed output at a time and expect a build. They also hold the other
# direction: an unchanged tree is skipped, and a run without ENABLE_QML does
# not wait for the outputs only an ENABLE_QML=true build has.
#
# Nothing is compiled and the caller's tree is not written to. The script under
# test is copied into a scratch git repository, because get_current_git_state
# reports "unknown" outside a work-tree root and should_skip_build never skips
# that state. The stand-ins are written beside the copy and put first in PATH:
# cmake creates the files a successful build leaves, and uname names the kernel
# each scenario asks for, so the Linux native block and the macOS block can
# both run on one host. Everything lives in one mktemp directory, removed on
# exit.
#
# Usage: lint-build-targets-skip.sh [build_targets.sh to test]
# Without an argument the build_targets.sh of this checkout is tested.
#
# To cover another output, make the cmake stand-in create it and add it to the
# outputs target_scenarios deletes.

export LC_ALL=C

# bash reads BASH_ENV again for every script it starts, which could put a
# directory ahead of the stand-ins.
unset BASH_ENV

NAME=$(basename "${BASH_SOURCE[0]}" .sh)
SOURCE=${1:-$(dirname "${BASH_SOURCE[0]}")/../../build_targets.sh}
if [ ! -f "${SOURCE}" ]; then
    echo "${NAME}: ${SOURCE} is missing."
    exit 1
fi

ROOT=$(mktemp -d "${TMPDIR:-/tmp}/${NAME}.XXXXXX")
if [ -z "${ROOT}" ] || [ ! -d "${ROOT}" ]; then
    echo "${NAME}: could not create a scratch directory."
    exit 1
fi
# An absolute path, so that the stand-ins stay first in PATH after the cd into
# the scratch tree. Keep this after the check above: with ROOT empty it would
# name the caller's directory, which the trap would then remove.
case "${ROOT}" in /*) ;; *) ROOT="${PWD}/${ROOT}" ;; esac
trap 'rm -rf "${ROOT}"' EXIT

TREE="${ROOT}/tree"
STUBS="${ROOT}/stubs"
OUT="${ROOT}/output.txt"
export STUB_LOG="${ROOT}/calls.log"
export STUB_BREW="${ROOT}/brew"
mkdir -p "${TREE}" "${STUBS}" "${STUB_BREW}/qtbase"
cp "${SOURCE}" "${TREE}/build_targets.sh"

# git runs a hook with GIT_INDEX_FILE exported, and in a linked work tree with
# GIT_DIR as well. Drop those variables, so that the git commands below and
# the ones build_targets.sh runs act on the scratch repository only.
for VARIABLE in $(git rev-parse --local-env-vars); do
    unset "${VARIABLE}"
done

git -C "${TREE}" init -q > /dev/null 2>&1
# No hook of the caller's git configuration (a global hooks directory, a
# template) runs in the scratch repository.
git -C "${TREE}" config core.hooksPath /dev/null
git -C "${TREE}" add build_targets.sh
git -C "${TREE}" -c user.name=lint -c user.email=lint@localhost -c commit.gpgsign=false commit -q -m "scratch copy"
STATE=$(git -C "${TREE}" rev-parse --short=12 HEAD 2> /dev/null)
if [ -z "${STATE}" ]; then
    echo "${NAME}: could not commit the scratch copy."
    exit 1
fi

cat > "${STUBS}/cmake" << 'EOF'
#!/usr/bin/env bash
# "cmake -B <dir> ..." records the configuration in <dir>. "cmake --build <dir>"
# creates the files a successful build of that configuration leaves there.
echo "cmake $*" >> "${STUB_LOG}"
CONFIGURE_DIR=""
BUILD_DIR=""
QML="OFF"
TESTS="OFF"
PREVIOUS=""
for ARGUMENT in "$@"; do
    case "${PREVIOUS}" in
        -B) CONFIGURE_DIR="${ARGUMENT}" ;;
        --build) BUILD_DIR="${ARGUMENT}" ;;
    esac
    # A repeated option keeps its last value here.
    case "${ARGUMENT}" in
        -DENABLE_QML=*) QML="${ARGUMENT#*=}" ;;
        -DENABLE_TESTS=*) TESTS="${ARGUMENT#*=}" ;;
    esac
    PREVIOUS="${ARGUMENT}"
done
if [ -n "${CONFIGURE_DIR}" ]; then
    mkdir -p "${CONFIGURE_DIR}"
    echo "${QML} ${TESTS}" > "${CONFIGURE_DIR}/.stub_configuration"
fi
if [ -n "${BUILD_DIR}" ]; then
    read -r QML TESTS < "${BUILD_DIR}/.stub_configuration"
    mkdir -p "${BUILD_DIR}/bin"
    touch "${BUILD_DIR}/bin/gridcoinresearchd"
    if [ "${BUILD_DIR}" = "build_macos" ]; then
        mkdir -p "${BUILD_DIR}/bin/gridcoinresearch.app/Contents/MacOS"
        touch "${BUILD_DIR}/bin/gridcoinresearch.app/Contents/MacOS/gridcoinresearch"
    else
        touch "${BUILD_DIR}/bin/gridcoinresearch"
    fi
    if [ "${QML}" = "ON" ]; then
        touch "${BUILD_DIR}/bin/gridcoinresearch-qml"
        if [ "${TESTS}" = "ON" ]; then
            mkdir -p "${BUILD_DIR}/qmltest"
            touch "${BUILD_DIR}/qmltest/test_gridcoin-qml"
        fi
    fi
fi
EOF

cat > "${STUBS}/ctest" << 'EOF'
#!/usr/bin/env bash
echo "ctest $*" >> "${STUB_LOG}"
EOF

cat > "${STUBS}/uname" << 'EOF'
#!/usr/bin/env bash
# build_targets.sh asks for the kernel name only (uname -s).
echo "${STUB_KERNEL}"
EOF

cat > "${STUBS}/brew" << 'EOF'
#!/usr/bin/env bash
# "brew --prefix" and "brew --prefix <formula>".
echo "${STUB_BREW}${2:+/$2}"
EOF

chmod +x "${STUBS}/cmake" "${STUBS}/ctest" "${STUBS}/uname" "${STUBS}/brew"

export PATH="${STUBS}:${PATH}"
for TOOL in cmake ctest uname brew; do
    if [ "$(command -v "${TOOL}")" != "${STUBS}/${TOOL}" ]; then
        echo "${NAME}: the ${TOOL} stand-in is not what a script would run (is ${ROOT} on a noexec mount? set TMPDIR); nothing was run."
        exit 1
    fi
done

CHECKS=0
FAILURES=0
RC=0
BLOCK=""
DIR=""

# run_script <kernel> <build_targets.sh argument>...
run_script() {
    local KERNEL=$1
    shift
    : > "${STUB_LOG}"
    (cd "${TREE}" && STUB_KERNEL="${KERNEL}" bash ./build_targets.sh SKIP_DEPS=true "$@") > "${OUT}" 2>&1
    RC=$?
}

fail() {
    FAILURES=$((FAILURES + 1))
    echo "FAIL: ${BLOCK}, $1"
}

# expect_build <scenario> <ON|OFF: the ENABLE_QML value of the run>
expect_build() {
    local EXPECTED="${STATE}"
    local RECORDED
    if [ "$2" = "ON" ]; then EXPECTED="${STATE}+qml"; fi
    RECORDED=$(cat "${TREE}/${DIR}/.build_state" 2> /dev/null)
    CHECKS=$((CHECKS + 1))
    if [ "${RC}" -ne 0 ]; then
        fail "$1: build_targets.sh exited ${RC}:"
        cat "${OUT}"
    elif grep -q -F "Build up-to-date" "${OUT}"; then
        fail "$1: skipped as up to date, expected a build"
    elif ! grep -q -e "^cmake -B ${DIR} .*-DENABLE_QML=$2" "${STUB_LOG}"; then
        fail "$1: no configure call with -DENABLE_QML=$2"
    elif [ "${RECORDED}" != "${EXPECTED}" ]; then
        fail "$1: recorded build state '${RECORDED}', expected '${EXPECTED}'"
    fi
}

# expect_skip <scenario> <ON|OFF: the ENABLE_QML value of the run>
expect_skip() {
    local EXPECTED="${STATE}"
    if [ "$2" = "ON" ]; then EXPECTED="${STATE}+qml"; fi
    CHECKS=$((CHECKS + 1))
    if [ "${RC}" -ne 0 ]; then
        fail "$1: build_targets.sh exited ${RC}:"
        cat "${OUT}"
    elif ! grep -q -x -F ">>> Build up-to-date (${EXPECTED}). Skipping..." "${OUT}"; then
        fail "$1: not skipped, expected 'Build up-to-date (${EXPECTED})'"
    elif [ -s "${STUB_LOG}" ]; then
        fail "$1: skipped as up to date, yet cmake or ctest was called"
    fi
}

# target_scenarios <kernel> <TARGET value> <build directory> <name of the block>
target_scenarios() {
    local KERNEL=$1
    local TARGET="TARGET=$2"
    local OUTPUT
    DIR=$3
    BLOCK=$4

    rm -rf "${TREE:?}/${DIR}"

    run_script "${KERNEL}" "${TARGET}" ENABLE_QML=true
    expect_build "ENABLE_QML=true, first run" ON

    run_script "${KERNEL}" "${TARGET}" ENABLE_QML=true
    expect_skip "ENABLE_QML=true, rerun with nothing changed" ON

    for OUTPUT in qmltest/test_gridcoin-qml bin/gridcoinresearch-qml; do
        rm -f "${TREE}/${DIR}/${OUTPUT}"
        run_script "${KERNEL}" "${TARGET}" ENABLE_QML=true
        expect_build "ENABLE_QML=true, rerun after ${DIR}/${OUTPUT} was deleted" ON
        # Put the file back whatever that run did, so that the next deletion
        # leaves exactly one output missing.
        mkdir -p "$(dirname "${TREE}/${DIR}/${OUTPUT}")"
        touch "${TREE}/${DIR}/${OUTPUT}"
    done

    run_script "${KERNEL}" "${TARGET}"
    expect_build "ENABLE_QML off after an ENABLE_QML=true build" OFF

    rm -f "${TREE}/${DIR}/bin/gridcoinresearch-qml" "${TREE}/${DIR}/qmltest/test_gridcoin-qml"
    run_script "${KERNEL}" "${TARGET}"
    expect_skip "ENABLE_QML off, rerun without the QML outputs" OFF
}

target_scenarios Linux native build "Linux native"
target_scenarios Darwin macos build_macos "macOS"

if [ "${FAILURES}" -ne 0 ]; then
    echo "${NAME}: ${FAILURES} of ${CHECKS} scenarios failed."
    exit 1
fi
echo "${NAME}: ${CHECKS} scenarios passed."
exit 0
