#!/usr/bin/env bash
#
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=ci_win64
export DOCKER_NAME_TAG=ubuntu:20.04  # Check that focal can cross-compile to win64
export HOST=x86_64-w64-mingw32
export PACKAGES="python3 nsis g++-mingw-w64-x86-64 wine-binfmt wine-stable winehq-stable"
export DPKG_ADD_ARCH="i386"
export GOAL="deploy"
export GRIDCOIN_CONFIG="--enable-reduce-exports --with-gui=qt5"
export NEED_XVFB="true"
