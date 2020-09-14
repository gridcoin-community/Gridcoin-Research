#!/usr/bin/env bash
#
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_macos_cross
export HOST=x86_64-apple-darwin16
export PACKAGES="cmake imagemagick libcap-dev librsvg2-bin libz-dev libbz2-dev libtiff-tools python python3-dev python3-setuptools python3-distutils"
export XCODE_VERSION=11.3.1
export XCODE_BUILD_ID=11C505
export RUN_UNIT_TESTS=false
export RUN_FUNCTIONAL_TESTS=false
export GOAL=""
export DEP_OPTS="NO_QT=1"
export GRIDCOIN_CONFIG="--enable-reduce-exports"
