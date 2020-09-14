#!/usr/bin/env bash
#
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_linux_i386
export DOCKER_NAME_TAG=ubuntu:20.04
export HOST=i686-pc-linux-gnu
export PACKAGES="python3 g++-multilib"
export RUN_UNIT_TESTS=true
export RUN_FUNCTIONAL_TESTS=false
# export RUN_SECURITY_TESTS="true"
export GOAL="install"
export GRIDCOIN_CONFIG="--enable-reduce-exports"
export DEP_OPTS="NO_QT=1"
export DPKG_ADD_ARCH="i386"
