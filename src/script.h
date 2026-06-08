// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_H
#define BITCOIN_SCRIPT_H

// Backward-compatibility shim: include all script/ headers plus wallet-layer
// headers that the old monolithic script.h pulled in. New code should include
// the specific script/ header it needs instead of this file.

#include "script/script.h"
#include "script/interpreter.h"
#include "script/sign.h"
#include "script/standard.h"

#include "keystore.h"

#endif // BITCOIN_SCRIPT_H
