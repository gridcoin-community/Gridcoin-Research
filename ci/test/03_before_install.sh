#!/usr/bin/env bash
#
# Copyright (c) 2018-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

BEGIN_FOLD () {
  echo ""
  CURRENT_FOLD_NAME=$1
  echo "::group::${CURRENT_FOLD_NAME}"
}

END_FOLD () {
  RET=$?
    echo "::endgroup::"
  if [ $RET != 0 ]; then
    echo "${CURRENT_FOLD_NAME} failed with status code ${RET}"
  fi
}

