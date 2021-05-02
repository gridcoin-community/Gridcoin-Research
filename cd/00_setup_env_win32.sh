#!/usr/bin/env bash
#
# Copyright (c) 2021 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_win32
export DOCKER_NAME_TAG=ubuntu:20.04
export HOST=i686-w64-mingw32
export PACKAGES="python3 nsis g++-mingw-w64-i686 wine-binfmt wine32"
export RUN_UNIT_TESTS=true
export RUN_FUNCTIONAL_TESTS=false
# export RUN_SECURITY_TESTS="true"
export GOAL="deploy"
export GRIDCOIN_CONFIG="--enable-reduce-exports"
export DPKG_ADD_ARCH="i386"
