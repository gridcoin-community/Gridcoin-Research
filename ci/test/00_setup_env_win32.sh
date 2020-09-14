#!/usr/bin/env bash
#
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_win32
export DOCKER_NAME_TAG=ubuntu:18.04  # Check that bionic can cross-compile to win32 (bionic is used in the gitian build as well)
export HOST=i686-w64-mingw32
export PACKAGES="python3 nsis g++-mingw-w64-i686 wine-binfmt wine32"
export RUN_UNIT_TESTS=true
export RUN_FUNCTIONAL_TESTS=false
# export RUN_SECURITY_TESTS="true"
export GOAL=""
export GRIDCOIN_CONFIG="--enable-reduce-exports"
export DEP_OPTS="NO_QT=1"
export DPKG_ADD_ARCH="i386"
