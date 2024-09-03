#!/usr/bin/env bash
#
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export HOST=aarch64-apple-darwin
export GOAL="install"
export GRIDCOIN_CONFIG="--with-gui --enable-reduce-exports"
export NEED_XVFB="true"
export NO_DEPENDS=1
export OSX_SDK=""
export CCACHE_SIZE=300M
export RUN_UNIT_TESTS=true
export RUN_FUNCTIONAL_TESTS=false
