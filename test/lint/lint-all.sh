#!/usr/bin/env bash
#
# Copyright (c) 2017-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# This script runs all contrib/devtools/lint-*.sh files, and fails if any exit
# with a non-zero status code.

# This script is intentionally locale dependent by not setting "export LC_ALL=C"
# in order to allow for the executed lint scripts to opt in or opt out of locale
# dependence themselves.

set -u

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")
LINTALL=$(basename "${BASH_SOURCE[0]}")

EXIT_CODE=0

IGNORE_EXIT=(
  "lint-assertions.sh"
  "lint-circular-dependencies.sh"
  "lint-format-strings.sh"
  "lint-includes.sh"
  "lint-python-dead-code.sh"
  "lint-python.sh"
  "lint-tests.sh"
)

for f in "${SCRIPTDIR}"/lint-*.sh; do
  if [ "$(basename "$f")" != "$LINTALL" ]; then
    if ! "$f"; then
      echo "^---- failure generated from $f"
      if ! python -c "import sys; sys.exit(int(sys.argv[1] not in sys.argv[2:]))" $(basename "$f") "${IGNORE_EXIT[@]}"; then
        EXIT_CODE=1
      fi
    fi
  fi
done

exit ${EXIT_CODE}
