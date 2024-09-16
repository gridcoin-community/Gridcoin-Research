#!/usr/bin/env bash
#
# Copyright (c) 2019-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export CONTAINER_NAME=cd_macos_arm64_cross
export DOCKER_NAME_TAG=ubuntu:24.04
export HOST=arm64-apple-darwin
export PACKAGES="clang lld llvm cmake imagemagick libcap-dev librsvg2-bin libz-dev libbz2-dev libtiff-tools python3-dev python3-setuptools xorriso"
export XCODE_VERSION=15.0
export XCODE_BUILD_ID=15A240d
export GRIDCOIN_CONFIG="--with-gui --enable-reduce-exports"
