#!/usr/bin/env bash
#
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export HOST=x86_64-apple-darwin16
export DOCKER_NAME_TAG=ubuntu:18.04  # Check that bionic can cross-compile to macos (bionic is used in the gitian build as well)
export GOAL="install"
export GRIDCOIN_CONFIG="--with-gui=qt5 --enable-reduce-exports"
export NEED_XVFB="true"
export NO_DEPENDS=1
export OSX_SDK=""
export CCACHE_SIZE=300M
export RUN_UNIT_TESTS=true
export RUN_FUNCTIONAL_TESTS=false
