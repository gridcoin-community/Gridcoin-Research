#!/usr/bin/env bash
#
# Copyright (c) 2021 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

DOCKER_EXEC mv release/* /release

if [[ $HOST = *-apple-* ]]; then
	KV=$(cat $BASE_ROOT_DIR/depends/hosts/darwin.mk | grep "OSX_MIN_VERSION=")
	VER=${KV#OSX_MIN_VERSION=}
	for f in /tmp/release/*.dmg; do
		mv $f ${f%.dmg}-min-$VER.dmg
	done
fi

cd /tmp/release/ || { echo "Failure"; exit 1; }
for f in *; do
    sha256sum $f > $f.SHA256
done
