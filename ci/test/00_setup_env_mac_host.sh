#!/usr/bin/env bash
#
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export HOST=x86_64-apple-darwin18
export GOAL="install"
# We run the contrib/install_db4.sh script rather than installing the
# Homebrew berkeley-db4 formula to add the Berkeley DB 4.8 dependency
# to avoid a quirk with macOS on GitHub Actions. This script compiles
# BDB with the "--disable-replication" flag. The tests failed because
# BDB complained that "replication requires locking support". We need
# to point the compiler at the local BDB installation:
export BDB_PREFIX="${BASE_ROOT_DIR}/db4"
export GRIDCOIN_CONFIG="--with-gui --enable-reduce-exports BDB_LIBS='-L${BDB_PREFIX}/lib -ldb_cxx-4.8' BDB_CFLAGS='-I${BDB_PREFIX}/include'"
export NEED_XVFB="true"
export NO_DEPENDS=1
export OSX_SDK=""
export CCACHE_SIZE=300M
export RUN_UNIT_TESTS=true
export RUN_FUNCTIONAL_TESTS=false
