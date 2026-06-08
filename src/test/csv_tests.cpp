// Copyright (c) 2024-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "key.h"
#include "main.h"
#include "primitives/transaction.h"
#include "script.h"

using namespace std;

extern uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType);

static CScript BuildCSVScript(int64_t nSequence)
{
    // Script fragment: <nSequence> OP_CHECKSEQUENCEVERIFY OP_DROP
    CScript script;
    script << CScriptNum(nSequence);
    script << OP_CHECKSEQUENCEVERIFY;
    script << OP_DROP;
    return script;
}

BOOST_AUTO_TEST_SUITE(csv_tests)

BOOST_AUTO_TEST_CASE(csv_basic_evaluation)
{
    // Test that CSV opcode works with matching nSequence
    CKey key;
    key.MakeNewKey(true);
    CPubKey pubkey = key.GetPubKey();

    CScript scriptPubKey;
    scriptPubKey << CScriptNum(10); // require 10 blocks relative locktime
    scriptPubKey << OP_CHECKSEQUENCEVERIFY;
    scriptPubKey << OP_DROP;
    scriptPubKey << pubkey;
    scriptPubKey << OP_CHECKSIG;

    // Create a spending transaction
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.vin.resize(1);
    tx.vin[0].nSequence = 10; // matches the script requirement
    tx.vout.resize(1);
    tx.vout[0].nValue = 1;

    // Sign
    uint256 hash = SignatureHash(scriptPubKey, CTransaction(tx), 0, SIGHASH_ALL);
    vector<unsigned char> vchSig;
    key.Sign(hash, vchSig);
    vchSig.push_back((unsigned char)SIGHASH_ALL);

    CScript scriptSig;
    scriptSig << vchSig;

    vector<vector<unsigned char>> stack;
    unsigned int flags = SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
    BOOST_CHECK(EvalScript(stack, scriptSig + scriptPubKey, flags, CTransaction(tx), 0));
}

BOOST_AUTO_TEST_CASE(csv_negative_operand_fails)
{
    // CSV with a negative operand should fail
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.vin.resize(1);
    tx.vin[0].nSequence = 10;
    tx.vout.resize(1);

    CScript scriptPubKey;
    scriptPubKey << CScriptNum(-1); // negative
    scriptPubKey << OP_CHECKSEQUENCEVERIFY;

    CScript scriptSig;

    vector<vector<unsigned char>> stack;
    unsigned int flags = SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
    // Should fail because the operand is negative
    BOOST_CHECK(!EvalScript(stack, scriptSig + scriptPubKey, flags, CTransaction(tx), 0));
}

BOOST_AUTO_TEST_CASE(csv_disabled_flag_passes)
{
    // When the disable flag is set on the operand, CSV acts as NOP
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.vin.resize(1);
    tx.vin[0].nSequence = 0; // doesn't matter
    tx.vout.resize(1);

    CScript scriptPubKey;
    scriptPubKey << CScriptNum((int64_t)(SEQUENCE_LOCKTIME_DISABLE_FLAG));
    scriptPubKey << OP_CHECKSEQUENCEVERIFY;
    scriptPubKey << OP_DROP; // Clean up the stack
    scriptPubKey << OP_TRUE; // Leave true on stack

    CScript scriptSig;

    vector<vector<unsigned char>> stack;
    unsigned int flags = SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
    BOOST_CHECK(EvalScript(stack, scriptSig + scriptPubKey, flags, CTransaction(tx), 0));
}

BOOST_AUTO_TEST_CASE(csv_version1_tx_fails)
{
    // CSV should fail when tx.nVersion < 2
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.vin.resize(1);
    tx.vin[0].nSequence = 10;
    tx.vout.resize(1);

    CScript scriptPubKey;
    scriptPubKey << CScriptNum(10);
    scriptPubKey << OP_CHECKSEQUENCEVERIFY;

    CScript scriptSig;

    vector<vector<unsigned char>> stack;
    unsigned int flags = SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
    BOOST_CHECK(!EvalScript(stack, scriptSig + scriptPubKey, flags, CTransaction(tx), 0));
}

BOOST_AUTO_TEST_CASE(csv_when_flag_not_set_acts_as_nop)
{
    // When SCRIPT_VERIFY_CHECKSEQUENCEVERIFY flag is not set, CSV acts as NOP3
    CMutableTransaction tx;
    tx.nVersion = 1; // doesn't matter since flag not set
    tx.vin.resize(1);
    tx.vin[0].nSequence = 0;
    tx.vout.resize(1);

    CScript scriptPubKey;
    scriptPubKey << CScriptNum(10);
    scriptPubKey << OP_CHECKSEQUENCEVERIFY;
    scriptPubKey << OP_DROP;
    scriptPubKey << OP_TRUE;

    CScript scriptSig;

    vector<vector<unsigned char>> stack;
    // No CSV flag, but no DISCOURAGE_UPGRADABLE_NOPS either
    unsigned int flags = SCRIPT_VERIFY_NONE;
    BOOST_CHECK(EvalScript(stack, scriptSig + scriptPubKey, flags, CTransaction(tx), 0));
}

BOOST_AUTO_TEST_CASE(csv_type_mismatch_fails)
{
    // Script requires time-based lock, but tx nSequence is height-based
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.vin.resize(1);
    tx.vin[0].nSequence = 10; // height-based (no TYPE_FLAG)
    tx.vout.resize(1);

    CScript scriptPubKey;
    // Require time-based: TYPE_FLAG set
    scriptPubKey << CScriptNum((int64_t)(SEQUENCE_LOCKTIME_TYPE_FLAG | 10));
    scriptPubKey << OP_CHECKSEQUENCEVERIFY;

    CScript scriptSig;

    vector<vector<unsigned char>> stack;
    unsigned int flags = SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
    BOOST_CHECK(!EvalScript(stack, scriptSig + scriptPubKey, flags, CTransaction(tx), 0));
}

BOOST_AUTO_TEST_SUITE_END()
