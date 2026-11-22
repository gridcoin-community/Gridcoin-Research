#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# OpenSSL and libcurl are initialised in one place, util::TlsLibrariesScope,
# as the first thing each program does after SetProcessNonDumpable(). The first
# OpenSSL initialisation call in a process decides whether openssl.cnf is read,
# so a second place that initialises either library, or a program that leaves
# the scope out, would let that decision be made elsewhere. Neither produces a
# compiler diagnostic, so it is checked here.

export LC_ALL=C

EXIT_CODE=0

OUTSIDE=$(git grep -n -E "curl_global_init|OPENSSL_init_(crypto|ssl)" -- "src/*.cpp" "src/*.h" \
    ":(exclude)src/util/tls_libraries.cpp" ":(exclude)src/util/tls_libraries.h" \
    ":(exclude)src/test/openssl_noconfig_probe.cpp" \
    ":(exclude)src/secp256k1" ":(exclude)src/leveldb" ":(exclude)src/univalue" \
    ":(exclude)src/crc32c" ":(exclude)src/bdb53" ":(exclude)src/ipc/libmultiprocess")
if [[ ${OUTSIDE} != "" ]]; then
    echo "OpenSSL or libcurl is initialised outside util::TlsLibrariesScope:"
    echo "${OUTSIDE}"
    EXIT_CODE=1
fi

for PROGRAM in src/gridcoinresearchd.cpp src/qt/bitcoin.cpp; do
    if ! grep -q "const util::TlsLibrariesScope " "${PROGRAM}"; then
        echo "${PROGRAM} does not construct util::TlsLibrariesScope in main()."
        EXIT_CODE=1
    fi
done

exit ${EXIT_CODE}
