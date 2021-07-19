#!/usr/bin/env bash
#
# Copyright (c) 2021 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

DOCKER_EXEC mv release/* /release

for f in /tmp/release/*; do
    sha256sum $f > $f.SHA256
done
