#!/usr/bin/env bash
#
# Copyright (c) 2018-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

export LC_ALL=C

./ci/retry/retry sudo apt update && sudo apt install -y clang-format-9
sudo update-alternatives --install /usr/bin/clang-format      clang-format      $(which clang-format-9     ) 100
sudo update-alternatives --install /usr/bin/clang-format-diff clang-format-diff $(which clang-format-diff-9) 100

./ci/retry/retry pip3 install codespell==2.0.0
./ci/retry/retry pip3 install flake8==3.8.3
./ci/retry/retry pip3 install yq
./ci/retry/retry pip3 install mypy==0.781
./ci/retry/retry pip3 install vulture==2.3

SHELLCHECK_VERSION=v0.7.1
curl -sL "https://github.com/koalaman/shellcheck/releases/download/${SHELLCHECK_VERSION}/shellcheck-${SHELLCHECK_VERSION}.linux.x86_64.tar.xz" | tar --xz -xf - --directory /tmp/
export PATH="/tmp/shellcheck-${SHELLCHECK_VERSION}:${PATH}"
