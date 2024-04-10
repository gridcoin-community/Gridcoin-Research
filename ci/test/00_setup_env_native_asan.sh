#!/usr/bin/env bash
#
# Copyright (c) 2019-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_native_asan
export PACKAGES="clang llvm libqt5gui5 libqt5core5a qtbase5-dev libqt5dbus5 qttools5-dev qttools5-dev-tools libssl-dev libevent-dev bsdmainutils libboost-system-dev libboost-filesystem-dev libboost-iostreams-dev libboost-test-dev libboost-thread-dev libdb5.3++-dev libminiupnpc-dev libqrencode-dev libzip-dev zlib1g zlib1g-dev libcurl4 libcurl4-openssl-dev"
export DOCKER_NAME_TAG=ubuntu:22.04
export NO_DEPENDS=1
export GOAL="install"
export GRIDCOIN_CONFIG="--with-incompatible-bdb --with-gui=qt5 CPPFLAGS='-DARENA_DEBUG -DDEBUG_LOCKORDER' --with-sanitizers=address,integer,undefined CC=clang CXX=clang++"
