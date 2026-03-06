// Copyright (c) 2024-2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "htlc.h"
#include "init.h"
#include <key_io.h>
#include "main.h"
#include "policy/fees.h"
#include "primitives/transaction.h"
#include "rpc/protocol.h"
#include "server.h"
#include "streams.h"
#include "txdb.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <univalue.h>

using namespace std;

extern uint256 SignatureHash(CScript scriptCode, const CTransaction& txTo, unsigned int nIn, int nHashType);

UniValue createhtlc(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 4 || params.size() > 5)
        throw runtime_error(
            "createhtlc <receiver_addr> <sender_addr> <hash_hex> <timeout> [amount]\n"
            "\n"
            "Create a Hash Time-Locked Contract.\n"
            "\n"
            "Arguments:\n"
            "1. receiver_addr   (string, required) Address of the receiver (claim path)\n"
            "2. sender_addr     (string, required) Address of the sender (refund path)\n"
            "3. hash_hex        (string, required) SHA256 hash of the preimage (64 hex chars)\n"
            "4. timeout         (numeric, required) Absolute locktime for refund path\n"
            "5. amount          (numeric, optional) Amount in GRC to fund the HTLC\n"
            "\n"
            "Returns a JSON object with the P2SH address and redeem script.\n"
            "If amount is provided, funds the HTLC with a transaction.\n"
            + HelpRequiringPassphrase());

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Parse receiver address
    CTxDestination receiver_dest = DecodeDestination(params[0].get_str());
    if (!IsValidDestination(receiver_dest))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid receiver address");

    const CKeyID* receiver_keyid = std::get_if<CKeyID>(&receiver_dest);
    if (!receiver_keyid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Receiver must be a pubkey hash address");

    CPubKey receiver_pubkey;
    if (!pwalletMain->GetPubKey(*receiver_keyid, receiver_pubkey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Receiver public key not found in wallet");

    // Parse sender address
    CTxDestination sender_dest = DecodeDestination(params[1].get_str());
    if (!IsValidDestination(sender_dest))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid sender address");

    const CKeyID* sender_keyid = std::get_if<CKeyID>(&sender_dest);
    if (!sender_keyid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sender must be a pubkey hash address");

    CPubKey sender_pubkey;
    if (!pwalletMain->GetPubKey(*sender_keyid, sender_pubkey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Sender public key not found in wallet");

    // Parse hash
    string strHash = params[2].get_str();
    if (strHash.size() != 64)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Hash must be 64 hex characters (32 bytes)");
    vector<unsigned char> hash = ParseHex(strHash);
    if (hash.size() != 32)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Hash must be 32 bytes");

    // Parse timeout
    int64_t timeout = params[3].get_int64();
    if (timeout <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Timeout must be positive");

    // Create the HTLC redeem script
    CScript redeemScript = CreateHTLCScript(hash, receiver_pubkey, sender_pubkey, timeout);

    // Import the redeem script into the wallet
    CScriptID scriptID = redeemScript.GetID();
    if (!pwalletMain->AddCScript(redeemScript))
        throw JSONRPCError(RPC_WALLET_ERROR, "Failed to add redeem script to wallet");

    pwalletMain->SetAddressBookName(scriptID, "htlc");

    UniValue result(UniValue::VOBJ);
    result.pushKV("p2sh_address", EncodeDestination(scriptID));
    result.pushKV("redeem_script", HexStr(redeemScript));
    result.pushKV("sender_pubkey", HexStr(sender_pubkey));
    result.pushKV("receiver_pubkey", HexStr(receiver_pubkey));
    result.pushKV("hash", strHash);
    result.pushKV("timeout", timeout);

    // If amount is provided, create and send a funding transaction
    if (params.size() > 4) {
        CAmount nAmount = AmountFromValue(params[4]);
        if (nAmount <= 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Amount must be positive");

        EnsureWalletIsUnlocked();

        // Create a P2SH output paying to the HTLC
        CScript scriptPubKey;
        scriptPubKey.SetDestination(scriptID);

        // Send the transaction
        CWalletTx wtx;
        string strError = pwalletMain->SendMoney(scriptPubKey, nAmount, wtx, false);
        if (!strError.empty())
            throw JSONRPCError(RPC_WALLET_ERROR, strError);

        result.pushKV("txid", wtx.GetHash().GetHex());
    }

    return result;
}

UniValue claimhtlc(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 4)
        throw runtime_error(
            "claimhtlc <htlc_txid> <vout> <preimage_hex> <destination_addr>\n"
            "\n"
            "Claim an HTLC output using the preimage.\n"
            "\n"
            "Arguments:\n"
            "1. htlc_txid        (string, required) Transaction ID of the HTLC funding tx\n"
            "2. vout              (numeric, required) Output index of the HTLC\n"
            "3. preimage_hex      (string, required) The preimage (hex encoded)\n"
            "4. destination_addr  (string, required) Address to send claimed funds to\n"
            + HelpRequiringPassphrase());

    LOCK2(cs_main, pwalletMain->cs_wallet);
    EnsureWalletIsUnlocked();

    // Parse txid
    uint256 htlc_txid;
    htlc_txid.SetHex(params[0].get_str());

    // Parse vout
    unsigned int nVout = params[1].get_int();

    // Parse preimage
    vector<unsigned char> preimage = ParseHex(params[2].get_str());

    // Parse destination
    CTxDestination dest = DecodeDestination(params[3].get_str());
    if (!IsValidDestination(dest))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid destination address");

    // Look up the HTLC transaction
    CTransaction htlcTx;
    uint256 hashBlock;
    if (!GetTransaction(htlc_txid, htlcTx, hashBlock))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "HTLC transaction not found");

    if (nVout >= htlcTx.vout.size())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "vout index out of range");

    const CTxOut& htlcOutput = htlcTx.vout[nVout];

    // Get the redeem script from the wallet
    // The scriptPubKey should be P2SH: OP_HASH160 <hash> OP_EQUAL
    CScriptID scriptID;
    if (!htlcOutput.scriptPubKey.IsPayToScriptHash()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "HTLC output is not P2SH");
    }

    // Extract the script hash from the P2SH output
    vector<unsigned char> hashBytes(htlcOutput.scriptPubKey.begin() + 2,
                                    htlcOutput.scriptPubKey.begin() + 22);
    scriptID = CScriptID(uint160(hashBytes));

    CScript redeemScript;
    if (!pwalletMain->GetCScript(scriptID, redeemScript))
        throw JSONRPCError(RPC_WALLET_ERROR, "Redeem script not found in wallet. "
                           "Use createhtlc or addredeemscript first.");

    // Parse the HTLC script to get the receiver's pubkey
    vector<unsigned char> htlc_hash;
    CPubKey receiver_pubkey, sender_pubkey;
    int64_t timeout;
    if (!ParseHTLCScript(redeemScript, htlc_hash, receiver_pubkey, sender_pubkey, timeout))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Script is not a valid HTLC");

    // Get the receiver's private key
    CKey receiver_key;
    if (!pwalletMain->GetKey(receiver_pubkey.GetID(), receiver_key))
        throw JSONRPCError(RPC_WALLET_ERROR, "Receiver's private key not found in wallet");

    // Build the spending transaction
    CTransaction txSpend;
    txSpend.nVersion = CTransaction::CURRENT_VERSION;
    txSpend.nTime = GetAdjustedTime();
    txSpend.nLockTime = 0;

    // Input: the HTLC UTXO
    CTxIn txin(COutPoint(htlc_txid, nVout), CScript(), 0xfffffffe);
    txSpend.vin.push_back(txin);

    // Output: pay to destination minus fee
    CScript destScript;
    destScript.SetDestination(dest);

    CAmount nValue = htlcOutput.nValue;
    CAmount nFee = GetBaseFee(txSpend);
    if (nValue <= nFee)
        throw JSONRPCError(RPC_WALLET_ERROR, "HTLC output value too small to cover fee");

    txSpend.vout.push_back(CTxOut(nValue - nFee, destScript));

    // Sign: compute signature hash over the redeem script
    uint256 sighash = SignatureHash(redeemScript, txSpend, 0, SIGHASH_ALL);

    vector<unsigned char> vchSig;
    if (!receiver_key.Sign(sighash, vchSig))
        throw JSONRPCError(RPC_WALLET_ERROR, "Failed to sign transaction");
    vchSig.push_back((unsigned char)SIGHASH_ALL);

    // Build scriptSig: <sig> <preimage> OP_TRUE <redeemScript>
    txSpend.vin[0].scriptSig = CreateHTLCClaimScript(vchSig, preimage, redeemScript);

    // Verify the script evaluates correctly
    if (!VerifyScript(txSpend.vin[0].scriptSig, htlcOutput.scriptPubKey,
                      SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY |
                      SCRIPT_VERIFY_CHECKSEQUENCEVERIFY,
                      txSpend, 0))
    {
        throw JSONRPCError(RPC_VERIFY_ERROR, "Script verification failed. "
                           "Check that the preimage is correct.");
    }

    // Broadcast
    {
        CValidationState state;
        if (!AcceptToMemoryPool(mempool, txSpend, state, nullptr))
            throw JSONRPCError(RPC_TRANSACTION_ERROR, "Transaction rejected by mempool");
    }

    uint256 txHash = txSpend.GetHash();
    SyncWithWallets(txSpend, nullptr, true);
    RelayTransaction(txSpend, txHash);

    UniValue result(UniValue::VOBJ);
    result.pushKV("txid", txHash.GetHex());
    return result;
}

UniValue refundhtlc(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "refundhtlc <htlc_txid> <vout> <destination_addr>\n"
            "\n"
            "Refund an HTLC output after the timeout has passed.\n"
            "\n"
            "Arguments:\n"
            "1. htlc_txid        (string, required) Transaction ID of the HTLC funding tx\n"
            "2. vout              (numeric, required) Output index of the HTLC\n"
            "3. destination_addr  (string, required) Address to send refunded funds to\n"
            + HelpRequiringPassphrase());

    LOCK2(cs_main, pwalletMain->cs_wallet);
    EnsureWalletIsUnlocked();

    // Parse txid
    uint256 htlc_txid;
    htlc_txid.SetHex(params[0].get_str());

    // Parse vout
    unsigned int nVout = params[1].get_int();

    // Parse destination
    CTxDestination dest = DecodeDestination(params[2].get_str());
    if (!IsValidDestination(dest))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid destination address");

    // Look up the HTLC transaction
    CTransaction htlcTx;
    uint256 hashBlock;
    if (!GetTransaction(htlc_txid, htlcTx, hashBlock))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "HTLC transaction not found");

    if (nVout >= htlcTx.vout.size())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "vout index out of range");

    const CTxOut& htlcOutput = htlcTx.vout[nVout];

    // Get the redeem script from the wallet
    if (!htlcOutput.scriptPubKey.IsPayToScriptHash()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "HTLC output is not P2SH");
    }

    vector<unsigned char> hashBytes(htlcOutput.scriptPubKey.begin() + 2,
                                    htlcOutput.scriptPubKey.begin() + 22);
    CScriptID scriptID = CScriptID(uint160(hashBytes));

    CScript redeemScript;
    if (!pwalletMain->GetCScript(scriptID, redeemScript))
        throw JSONRPCError(RPC_WALLET_ERROR, "Redeem script not found in wallet. "
                           "Use createhtlc or addredeemscript first.");

    // Parse the HTLC script to get the sender's pubkey and timeout
    vector<unsigned char> htlc_hash;
    CPubKey receiver_pubkey, sender_pubkey;
    int64_t timeout;
    if (!ParseHTLCScript(redeemScript, htlc_hash, receiver_pubkey, sender_pubkey, timeout))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Script is not a valid HTLC");

    // Check that the timeout has passed
    int64_t nNow = GetAdjustedTime();
    if (timeout >= LOCKTIME_THRESHOLD) {
        // Time-based locktime
        if (nNow < timeout)
            throw JSONRPCError(RPC_VERIFY_ERROR,
                strprintf("HTLC timeout has not yet passed. Current time: %d, timeout: %d", nNow, timeout));
    } else {
        // Height-based locktime
        if (nBestHeight < timeout)
            throw JSONRPCError(RPC_VERIFY_ERROR,
                strprintf("HTLC timeout height has not yet been reached. Current height: %d, timeout: %d",
                          nBestHeight, (int)timeout));
    }

    // Get the sender's private key
    CKey sender_key;
    if (!pwalletMain->GetKey(sender_pubkey.GetID(), sender_key))
        throw JSONRPCError(RPC_WALLET_ERROR, "Sender's private key not found in wallet");

    // Build the spending transaction
    CTransaction txSpend;
    txSpend.nVersion = CTransaction::CURRENT_VERSION;
    txSpend.nTime = GetAdjustedTime();
    txSpend.nLockTime = timeout;

    // Input: the HTLC UTXO with nSequence < max to enable nLockTime
    CTxIn txin(COutPoint(htlc_txid, nVout), CScript(), 0xfffffffe);
    txSpend.vin.push_back(txin);

    // Output: pay to destination minus fee
    CScript destScript;
    destScript.SetDestination(dest);

    CAmount nValue = htlcOutput.nValue;
    CAmount nFee = GetBaseFee(txSpend);
    if (nValue <= nFee)
        throw JSONRPCError(RPC_WALLET_ERROR, "HTLC output value too small to cover fee");

    txSpend.vout.push_back(CTxOut(nValue - nFee, destScript));

    // Sign: compute signature hash over the redeem script
    uint256 sighash = SignatureHash(redeemScript, txSpend, 0, SIGHASH_ALL);

    vector<unsigned char> vchSig;
    if (!sender_key.Sign(sighash, vchSig))
        throw JSONRPCError(RPC_WALLET_ERROR, "Failed to sign transaction");
    vchSig.push_back((unsigned char)SIGHASH_ALL);

    // Build scriptSig: <sig> OP_FALSE <redeemScript>
    txSpend.vin[0].scriptSig = CreateHTLCRefundScript(vchSig, redeemScript);

    // Verify the script evaluates correctly
    if (!VerifyScript(txSpend.vin[0].scriptSig, htlcOutput.scriptPubKey,
                      SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY |
                      SCRIPT_VERIFY_CHECKSEQUENCEVERIFY,
                      txSpend, 0))
    {
        throw JSONRPCError(RPC_VERIFY_ERROR, "Script verification failed. "
                           "The timeout may not have been reached yet.");
    }

    // Broadcast
    {
        CValidationState state;
        if (!AcceptToMemoryPool(mempool, txSpend, state, nullptr))
            throw JSONRPCError(RPC_TRANSACTION_ERROR, "Transaction rejected by mempool");
    }

    uint256 txHash = txSpend.GetHash();
    SyncWithWallets(txSpend, nullptr, true);
    RelayTransaction(txSpend, txHash);

    UniValue result(UniValue::VOBJ);
    result.pushKV("txid", txHash.GetHex());
    return result;
}
