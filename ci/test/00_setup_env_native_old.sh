#!/usr/bin/env bash
#
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_native_old
export DOCKER_NAME_TAG=ubuntu:16.04
export PACKAGES="libqt5gui5 libqt5core5a qtbase5-dev libqt5dbus5 qttools5-dev qttools5-dev-tools libssl-dev libevent-dev bsdmainutils libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-iostreams-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev libdb5.3++-dev libminiupnpc-dev libqrencode-dev libzip-dev zlib1g zlib1g-dev libcurl4-openssl-dev"
export RUN_UNIT_TESTS=true
# export RUN_FUNCTIONAL_TESTS=false
# export RUN_SECURITY_TESTS="true"
export GOAL="install"
export GRIDCOIN_CONFIG="--enable-reduce-exports --with-incompatible-bdb --with-gui=qt5"
export NEED_XVFB="true"
export NO_DEPENDS=1
