#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# IPC auth-gate completeness lint (multiprocess design section 4.3): every
# virtual on interfaces::Init must have an override in ipc/serve_init.cpp's
# ServeInit wrapper, and every override must call RequireAuth(). A missing
# override fails closed but silently; an override without RequireAuth() fails
# open. Neither produces a compiler diagnostic, so it is checked here.

export LC_ALL=C
exec python3 "$(dirname "$0")/lint-serve-init-complete.py"
