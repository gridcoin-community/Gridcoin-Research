#!/usr/bin/env bash
#
# Copyright (c) 2022 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

export LC_ALL=C
set -e

if [[ -z "${COMMIT_RANGE}" ]]; then
  echo "USAGE: COMMIT_RANGE=\"FIRST..LAST\" $0"
  exit 0
fi

EXIT_CODE=0
while read f; do
  if grep -E "http://(www\.)?opensource.org" "$f" > /dev/null; then
    EXIT_CODE=1
    echo "$f: needs https correction for the copyright header."
  fi
done < <(git diff --name-only "${COMMIT_RANGE}" | grep -vE "src/(secp256k1|crc32c|leveldb|univalue|bdb53)")

exit ${EXIT_CODE}

