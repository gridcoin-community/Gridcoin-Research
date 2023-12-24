#!/usr/bin/env bash
#
# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

# latest_stage.log will contain logs for a silenced stage if it
# fails.
trap "cat latest_stage.log" EXIT

set -o errexit; source ./ci/test/00_setup_env.sh
set -o errexit; source ./ci/test/03_before_install.sh
set -o errexit; source ./ci/test/04_install.sh &> latest_stage.log
set -o errexit; source ./ci/test/05_before_script.sh &> latest_stage.log
echo -n > latest_stage.log
set -o errexit; source ./ci/test/06_script_a.sh
set -o errexit; source ./ci/test/06_script_b.sh
