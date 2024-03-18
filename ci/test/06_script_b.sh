#!/usr/bin/env bash
#
# Copyright (c) 2018-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

if [[ $HOST = *-mingw32 ]]; then
  BEGIN_FOLD wrap-wine
  # Generate all binaries, so that they can be wrapped
  # DOCKER_EXEC make $MAKEJOBS -C src/secp256k1 VERBOSE=1
  DOCKER_EXEC make $MAKEJOBS -C src/univalue VERBOSE=1
  DOCKER_EXEC "${BASE_ROOT_DIR}/ci/test/wrap-wine.sh"
  END_FOLD
fi

if [ -n "$QEMU_USER_CMD" ]; then
  BEGIN_FOLD wrap-qemu
  # Generate all binaries, so that they can be wrapped
  # DOCKER_EXEC make $MAKEJOBS -C src/secp256k1 VERBOSE=1
  DOCKER_EXEC make $MAKEJOBS -C src/univalue VERBOSE=1
  DOCKER_EXEC "${BASE_ROOT_DIR}/ci/test/wrap-qemu.sh"
  END_FOLD
fi

if [ -n "$USE_VALGRIND" ]; then
  BEGIN_FOLD wrap-valgrind
  DOCKER_EXEC "${BASE_ROOT_DIR}/ci/test/wrap-valgrind.sh"
  END_FOLD
fi

if [ "$RUN_UNIT_TESTS" = "true" ]; then
  BEGIN_FOLD unit-tests
  DOCKER_EXEC LD_LIBRARY_PATH=$DEPENDS_DIR/$HOST/lib make $MAKEJOBS check VERBOSE=1
  END_FOLD
fi

if [ "$RUN_UNIT_TESTS_SEQUENTIAL" = "true" ]; then
  BEGIN_FOLD unit-tests-seq
  DOCKER_EXEC LD_LIBRARY_PATH=$DEPENDS_DIR/$HOST/lib "${BASE_BUILD_DIR}/gridcoin-*/src/test/test_gridcoin*" --catch_system_errors=no -l test_suite
  END_FOLD
fi

if [ "$RUN_FUNCTIONAL_TESTS" = "true" ]; then
  BEGIN_FOLD functional-tests
  DOCKER_EXEC LD_LIBRARY_PATH=$DEPENDS_DIR/$HOST/lib ${TEST_RUNNER_ENV} test/functional/test_runner.py --ci $MAKEJOBS --tmpdirprefix "${BASE_SCRATCH_DIR}/test_runner/" --ansi --combinedlogslen=4000 ${TEST_RUNNER_EXTRA} --quiet --failfast
  END_FOLD
fi

if [ "$RUN_SECURITY_TESTS" = "true" ]; then
  BEGIN_FOLD security-tests
  DOCKER_EXEC make test-security-check
  END_FOLD
fi

if [ "$RUN_FUZZ_TESTS" = "true" ]; then
  BEGIN_FOLD fuzz-tests
  DOCKER_EXEC LD_LIBRARY_PATH=$DEPENDS_DIR/$HOST/lib test/fuzz/test_runner.py ${FUZZ_TESTS_CONFIG} $MAKEJOBS -l DEBUG ${DIR_FUZZ_IN}
  END_FOLD
fi

if [ -n "$GITHUB_ENV" ]; then
    FILES=

    ARTIFACT_FILENAME="gridcoin-$(DOCKER_EXEC make print-VERSION | cut -d = -f 2-)-$(git rev-parse --short HEAD)-$CONTAINER_NAME.tar.gz"
    DAEMON_BIN="$(DOCKER_EXEC make print-BITCOIND_BIN | cut -d = -f 2-)"
    QT_BIN="$(DOCKER_EXEC make print-BITCOIN_QT_BIN | cut -d = -f 2-)"
    WIN_INSTALLER="$(DOCKER_EXEC make print-BITCOIN_WIN_INSTALLER | cut -d = -f 2-)"
    OSX_DMG="$(DOCKER_EXEC make print-OSX_DMG | cut -d = -f 2-)"

    if DOCKER_EXEC test -f "$DAEMON_BIN"; then
      FILES="$FILES $DAEMON_BIN"
    fi
    if DOCKER_EXEC test -f "$QT_BIN"; then
      FILES="$FILES $QT_BIN"
    fi
    if DOCKER_EXEC test -f "$WIN_INSTALLER"; then
      FILES="$FILES $WIN_INSTALLER"
    fi
    if DOCKER_EXEC test -f "$OSX_DMG"; then
      FILES="$FILES $OSX_DMG"
    fi

    DOCKER_EXEC tar --create --file="$DEPENDS_DIR/$ARTIFACT_FILENAME" --gzip --strip-components=2 $FILES
    echo "ARTIFACT_FILENAME=$ARTIFACT_FILENAME" >> $GITHUB_ENV
    echo "ARTIFACT_FILE=$DEPENDS_DIR/$ARTIFACT_FILENAME" >> $GITHUB_ENV
fi
