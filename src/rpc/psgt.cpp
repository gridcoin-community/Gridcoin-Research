// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <psgt.h>

#include <key_io.h>
#include <main.h>
#include <rpc/server.h>
#include <rpc/protocol.h>
#include <rpc/util.h>
#include <script/sign.h>
#include <script/standard.h>
#include <streams.h>
#include <util/bip32.h>
#include <util/strencodings.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <univalue.h>

using namespace std;

extern CWallet* pwalletMain;

/** Populate HD keypath info for a pubkey from wallet metadata. */
static void AddHDKeypathIfAvailable(const CWallet& wallet,
                                     const CPubKey& pubkey,
                                     std::map<CPubKey, KeyOriginInfo>& hd_keypaths)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet)
{
    CKeyID keyid = pubkey.GetID();
    auto it = wallet.mapKeyMetadata.find(keyid);
    if (it == wallet.mapKeyMetadata.end())
        return;

    const CKeyMetadata& meta = it->second;
    if (meta.hdKeypath.empty() || meta.hdMasterKeyID.IsNull())
        return;

    std::vector<uint32_t> path;
    if (!ParseHDKeypath(meta.hdKeypath, path))
        return;

    KeyOriginInfo info;
    // Fingerprint = first 4 bytes of the master key's Hash160
    // masterKeyID IS the Hash160, so take its first 4 bytes.
    memcpy(info.fingerprint, meta.hdMasterKeyID.begin(), 4);
    info.path = path;
    hd_keypaths[pubkey] = info;
}

/** Populate HD keypaths for all pubkeys involved in a scriptPubKey. */
static void FillHDKeypaths(const CWallet& wallet,
                            const CScript& scriptPubKey,
                            std::map<CPubKey, KeyOriginInfo>& hd_keypaths)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet)
{
    std::vector<CKeyID> vKeys;
    ExtractAffectedKeys(wallet, scriptPubKey, vKeys);
    for (const CKeyID& keyid : vKeys)
    {
        CPubKey pubkey;
        if (wallet.GetPubKey(keyid, pubkey))
            AddHDKeypathIfAvailable(wallet, pubkey, hd_keypaths);
    }
}

UniValue createpsgt(const UniValue& params, bool fHelp)
{
    static const RPCHelpMan help{
        "createpsgt",
        "Creates an unsigned Partially Signed Gridcoin Transaction (PSGT).",
        {
            {"inputs", RPCArg::Type::ARR, RPCArg::Optional::NO,
                "A JSON array of inputs.",
                {
                    {"input", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "",
                        {
                            {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The transaction id."},
                            {"vout", RPCArg::Type::NUM, RPCArg::Optional::NO, "The output number."},
                        }},
                }},
            {"outputs", RPCArg::Type::OBJ_USER_KEYS, RPCArg::Optional::NO,
                "A JSON object with addresses as keys and GRC amounts as values.",
                {
                    {"address", RPCArg::Type::AMOUNT, RPCArg::Optional::OMITTED,
                        "The GRC amount to send to this address."},
                }},
            {"ntime", RPCArg::Type::NUM, RPCArg::Optional::OMITTED,
                "Transaction timestamp (default: current adjusted time)."},
        },
        RPCResult{RPCResult::Type::STR, "psgt", "The base64-encoded unsigned PSGT."},
        RPCExamples{
            HelpExampleCli("createpsgt",
                "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"address\\\":0.01}\"") +
            HelpExampleRpc("createpsgt",
                "[{\"txid\":\"myid\",\"vout\":0}], {\"address\":0.01}")},
    };
    if (fHelp || !help.IsValidNumArgs(params.size()))
        throw runtime_error(help.ToString());

    RPCTypeCheck(params, {UniValue::VARR, UniValue::VOBJ});

    UniValue inputs = params[0].get_array();
    UniValue outputs = params[1].get_obj();

    CMutableTransaction mtx;
    mtx.nVersion = CTransaction::CURRENT_VERSION;

    if (params.size() > 2)
        mtx.nTime = params[2].get_int();

    for (unsigned int idx = 0; idx < inputs.size(); idx++)
    {
        const UniValue& input = inputs[idx];
        const UniValue& o = input.get_obj();

        uint256 txid;
        txid.SetHex(find_value(o, "txid").get_str());
        int nOutput = find_value(o, "vout").get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        CTxIn in(COutPoint(txid, nOutput));
        mtx.vin.push_back(in);
    }

    vector<string> addrList = outputs.getKeys();
    for (const string& name_ : addrList)
    {
        CTxDestination dest = DecodeDestination(name_);
        if (!IsValidDestination(dest))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Gridcoin address: ") + name_);

        CAmount nAmount = AmountFromValue(outputs[name_]);
        CScript scriptPubKey;
        scriptPubKey.SetDestination(dest);
        mtx.vout.push_back(CTxOut(nAmount, scriptPubKey));
    }

    // Ensure inputs have empty scriptSigs.
    for (auto& vin : mtx.vin)
        vin.scriptSig.clear();

    PartiallySignedTransaction psgt(mtx);

    std::vector<unsigned char> data = SerializePSGT(psgt);
    return EncodeBase64(data.data(), data.size());
}

UniValue decodepsgt(const UniValue& params, bool fHelp)
{
    static const RPCHelpMan help{
        "decodepsgt",
        "Return a JSON object representing the given PSGT (Partially Signed Gridcoin Transaction).",
        {
            {"psgt", RPCArg::Type::STR, RPCArg::Optional::NO,
                "The base64-encoded PSGT."},
        },
        RPCResult{RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::OBJ, "tx", "The decoded unsigned transaction (same shape as decoderawtransaction).",
                    {{RPCResult::Type::ELISION, "", ""}}},
                {RPCResult::Type::ARR, "inputs", "Per-input PSGT metadata.",
                    {{RPCResult::Type::ELISION, "", "input fields: non_witness_utxo, partial_signatures, sighash, redeem_script, final_scriptSig, bip32_derivs, unknown"}}},
                {RPCResult::Type::ARR, "outputs", "Per-output PSGT metadata.",
                    {{RPCResult::Type::ELISION, "", "output fields: redeem_script, bip32_derivs, unknown"}}},
            }},
        RPCExamples{
            HelpExampleCli("decodepsgt", "\"cHNidP8B...\"") +
            HelpExampleRpc("decodepsgt", "\"cHNidP8B...\"")},
    };
    if (fHelp || !help.IsValidNumArgs(params.size()))
        throw runtime_error(help.ToString());

    PartiallySignedTransaction psgt;
    string error;
    if (!DecodeRawPSGT(psgt, params[0].get_str(), error))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, error);

    UniValue result(UniValue::VOBJ);

    // Unsigned transaction.
    {
        UniValue tx_obj(UniValue::VOBJ);
        CTransaction ctx(psgt.tx);
        tx_obj.pushKV("txid", ctx.GetHash().GetHex());
        tx_obj.pushKV("version", psgt.tx.nVersion);
        tx_obj.pushKV("time", (int64_t)psgt.tx.nTime);
        tx_obj.pushKV("locktime", (int64_t)psgt.tx.nLockTime);

        UniValue vin_arr(UniValue::VARR);
        for (const auto& txin : psgt.tx.vin)
        {
            UniValue in_obj(UniValue::VOBJ);
            in_obj.pushKV("txid", txin.prevout.hash.GetHex());
            in_obj.pushKV("vout", (int)txin.prevout.n);
            in_obj.pushKV("sequence", (int64_t)txin.nSequence);
            vin_arr.push_back(in_obj);
        }
        tx_obj.pushKV("vin", vin_arr);

        UniValue vout_arr(UniValue::VARR);
        int n = 0;
        for (const auto& txout : psgt.tx.vout)
        {
            UniValue out_obj(UniValue::VOBJ);
            out_obj.pushKV("value", ValueFromAmount(txout.nValue));
            out_obj.pushKV("n", n++);

            UniValue script_obj(UniValue::VOBJ);
            script_obj.pushKV("asm", txout.scriptPubKey.ToString());
            script_obj.pushKV("hex", HexStr(txout.scriptPubKey));

            txnouttype type;
            vector<CTxDestination> addresses;
            int nRequired;
            if (ExtractDestinations(txout.scriptPubKey, type, addresses, nRequired))
            {
                script_obj.pushKV("type", GetTxnOutputType(type));
                UniValue addr_arr(UniValue::VARR);
                for (const auto& addr : addresses)
                    addr_arr.push_back(EncodeDestination(addr));
                script_obj.pushKV("addresses", addr_arr);
                if (nRequired > 1)
                    script_obj.pushKV("reqSigs", nRequired);
            }
            out_obj.pushKV("scriptPubKey", script_obj);
            vout_arr.push_back(out_obj);
        }
        tx_obj.pushKV("vout", vout_arr);

        result.pushKV("tx", tx_obj);
    }

    // Inputs.
    UniValue inputs_arr(UniValue::VARR);
    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        const PSGTInput& input = psgt.inputs[i];
        UniValue in_obj(UniValue::VOBJ);

        if (!input.non_witness_utxo.IsNull())
        {
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << input.non_witness_utxo;
            in_obj.pushKV("non_witness_utxo_hex", HexStr(ss));
        }

        if (!input.partial_sigs.empty())
        {
            UniValue sigs_obj(UniValue::VOBJ);
            for (const auto& sig : input.partial_sigs)
                sigs_obj.pushKV(sig.first.GetHex(), HexStr(sig.second));
            in_obj.pushKV("partial_signatures", sigs_obj);
        }

        if (input.sighash_type != 0)
            in_obj.pushKV("sighash", input.sighash_type);

        if (!input.redeem_script.empty())
        {
            UniValue rs_obj(UniValue::VOBJ);
            rs_obj.pushKV("asm", input.redeem_script.ToString());
            rs_obj.pushKV("hex", HexStr(input.redeem_script));
            in_obj.pushKV("redeem_script", rs_obj);
        }

        if (!input.final_script_sig.empty())
        {
            UniValue fs_obj(UniValue::VOBJ);
            fs_obj.pushKV("asm", input.final_script_sig.ToString());
            fs_obj.pushKV("hex", HexStr(input.final_script_sig));
            in_obj.pushKV("final_scriptSig", fs_obj);
        }

        if (!input.hd_keypaths.empty())
        {
            UniValue bip32_arr(UniValue::VARR);
            for (const auto& kp : input.hd_keypaths)
            {
                UniValue kp_obj(UniValue::VOBJ);
                kp_obj.pushKV("pubkey", HexStr(std::vector<unsigned char>(kp.first.begin(), kp.first.end())));
                kp_obj.pushKV("master_fingerprint", HexStr(std::vector<unsigned char>(kp.second.fingerprint, kp.second.fingerprint + 4)));
                kp_obj.pushKV("path", WriteHDKeypath(kp.second.path));
                bip32_arr.push_back(kp_obj);
            }
            in_obj.pushKV("bip32_derivs", bip32_arr);
        }

        if (!input.unknown.empty())
        {
            UniValue unk_obj(UniValue::VOBJ);
            for (const auto& entry : input.unknown)
                unk_obj.pushKV(HexStr(entry.first), HexStr(entry.second));
            in_obj.pushKV("unknown", unk_obj);
        }

        inputs_arr.push_back(in_obj);
    }
    result.pushKV("inputs", inputs_arr);

    // Outputs.
    UniValue outputs_arr(UniValue::VARR);
    for (unsigned int i = 0; i < psgt.outputs.size(); ++i)
    {
        const PSGTOutput& output = psgt.outputs[i];
        UniValue out_obj(UniValue::VOBJ);

        if (!output.redeem_script.empty())
        {
            UniValue rs_obj(UniValue::VOBJ);
            rs_obj.pushKV("asm", output.redeem_script.ToString());
            rs_obj.pushKV("hex", HexStr(output.redeem_script));
            out_obj.pushKV("redeem_script", rs_obj);
        }

        if (!output.hd_keypaths.empty())
        {
            UniValue bip32_arr(UniValue::VARR);
            for (const auto& kp : output.hd_keypaths)
            {
                UniValue kp_obj(UniValue::VOBJ);
                kp_obj.pushKV("pubkey", HexStr(std::vector<unsigned char>(kp.first.begin(), kp.first.end())));
                kp_obj.pushKV("master_fingerprint", HexStr(std::vector<unsigned char>(kp.second.fingerprint, kp.second.fingerprint + 4)));
                kp_obj.pushKV("path", WriteHDKeypath(kp.second.path));
                bip32_arr.push_back(kp_obj);
            }
            out_obj.pushKV("bip32_derivs", bip32_arr);
        }

        if (!output.unknown.empty())
        {
            UniValue unk_obj(UniValue::VOBJ);
            for (const auto& entry : output.unknown)
                unk_obj.pushKV(HexStr(entry.first), HexStr(entry.second));
            out_obj.pushKV("unknown", unk_obj);
        }

        outputs_arr.push_back(out_obj);
    }
    result.pushKV("outputs", outputs_arr);

    return result;
}

UniValue combinepsgt(const UniValue& params, bool fHelp)
{
    static const RPCHelpMan help{
        "combinepsgt",
        "Combine multiple PSGTs for the same transaction into one merged PSGT.",
        {
            {"psgts", RPCArg::Type::ARR, RPCArg::Optional::NO,
                "A JSON array of base64-encoded PSGT strings to combine.",
                {
                    {"psgt", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "A base64-encoded PSGT."},
                }},
        },
        RPCResult{RPCResult::Type::STR, "psgt", "The base64-encoded combined PSGT."},
        RPCExamples{
            HelpExampleCli("combinepsgt", "\"[\\\"cHNidP8B...\\\", \\\"cHNidP8B...\\\"]\"") +
            HelpExampleRpc("combinepsgt", "[\"cHNidP8B...\", \"cHNidP8B...\"]")},
    };
    if (fHelp || !help.IsValidNumArgs(params.size()))
        throw runtime_error(help.ToString());

    RPCTypeCheck(params, {UniValue::VARR});
    UniValue psgtArr = params[0].get_array();

    vector<PartiallySignedTransaction> psgts;
    for (unsigned int i = 0; i < psgtArr.size(); ++i)
    {
        PartiallySignedTransaction psgt;
        string error;
        if (!DecodeRawPSGT(psgt, psgtArr[i].get_str(), error))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, strprintf("PSGT decode failed at index %d: %s", i, error));
        psgts.push_back(std::move(psgt));
    }

    PartiallySignedTransaction merged;
    if (!CombinePSGTs(merged, psgts))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "PSGTs do not refer to the same transaction");

    vector<unsigned char> data = SerializePSGT(merged);
    return EncodeBase64(data.data(), data.size());
}

UniValue finalizepsgt(const UniValue& params, bool fHelp)
{
    static const RPCHelpMan help{
        "finalizepsgt",
        "Finalize a PSGT. If all inputs have been signed, extract the complete transaction and return it as hex.",
        {
            {"psgt", RPCArg::Type::STR, RPCArg::Optional::NO,
                "A base64-encoded PSGT."},
        },
        RPCResult{RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR_HEX, "hex", /*optional=*/true,
                    "The hex-encoded raw transaction (only present when complete)."},
                {RPCResult::Type::BOOL, "complete",
                    "Whether the transaction is fully signed and ready to broadcast."},
            }},
        RPCExamples{
            HelpExampleCli("finalizepsgt", "\"cHNidP8B...\"") +
            HelpExampleRpc("finalizepsgt", "\"cHNidP8B...\"")},
    };
    if (fHelp || !help.IsValidNumArgs(params.size()))
        throw runtime_error(help.ToString());

    PartiallySignedTransaction psgt;
    string error;
    if (!DecodeRawPSGT(psgt, params[0].get_str(), error))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, error);

    UniValue result(UniValue::VOBJ);

    CMutableTransaction mtx;
    bool complete = FinalizeAndExtractPSGT(psgt, mtx);

    if (complete)
    {
        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << mtx;
        result.pushKV("hex", HexStr(ssTx));
    }
    result.pushKV("complete", complete);

    return result;
}

UniValue walletprocesspsgt(const UniValue& params, bool fHelp)
{
    static const RPCHelpMan help{
        "walletprocesspsgt",
        "Sign a PSGT with keys from the wallet. "
        "Requires wallet passphrase to be set with walletpassphrase first if wallet is encrypted.",
        {
            {"psgt", RPCArg::Type::STR, RPCArg::Optional::NO,
                "A base64-encoded PSGT."},
            {"sighashtype", RPCArg::Type::STR, RPCArg::Optional::OMITTED,
                "The signature hash type to sign with (default: ALL). "
                "Valid: \"ALL\", \"ALL|ANYONECANPAY\", \"NONE\", \"NONE|ANYONECANPAY\", \"SINGLE\", \"SINGLE|ANYONECANPAY\"."},
        },
        RPCResult{RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "psgt", "The base64-encoded partially-signed transaction."},
                {RPCResult::Type::BOOL, "complete", "Whether the transaction has a complete set of signatures."},
            }},
        RPCExamples{
            HelpExampleCli("walletprocesspsgt", "\"cHNidP8B...\"") +
            HelpExampleRpc("walletprocesspsgt", "\"cHNidP8B...\", \"ALL\"")},
    };
    if (fHelp || !help.IsValidNumArgs(params.size()))
        throw runtime_error(help.ToString());

    EnsureWalletIsUnlocked();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    PartiallySignedTransaction psgt;
    string error;
    if (!DecodeRawPSGT(psgt, params[0].get_str(), error))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, error);

    // Parse sighash type.
    int nHashType = SIGHASH_ALL;
    if (params.size() > 1)
    {
        static map<string, int> mapSigHashValues = {
            { "ALL"                , SIGHASH_ALL                           },
            { "ALL|ANYONECANPAY"   , SIGHASH_ALL | SIGHASH_ANYONECANPAY    },
            { "NONE"               , SIGHASH_NONE                          },
            { "NONE|ANYONECANPAY"  , SIGHASH_NONE | SIGHASH_ANYONECANPAY   },
            { "SINGLE"             , SIGHASH_SINGLE                        },
            { "SINGLE|ANYONECANPAY", SIGHASH_SINGLE | SIGHASH_ANYONECANPAY },
        };
        string strHashType = params[1].get_str();
        if (!mapSigHashValues.count(strHashType))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid sighash param");
        nHashType = mapSigHashValues[strHashType];
    }

    // Fill in non_witness_utxo and redeem_script for inputs that need them.
    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        if (psgt.inputs[i].non_witness_utxo.IsNull())
        {
            const uint256& prevHash = psgt.tx.vin[i].prevout.hash;
            CTransaction prevTx;
            uint256 hashBlock;
            if (GetTransaction(prevHash, prevTx, hashBlock))
                psgt.inputs[i].non_witness_utxo = prevTx;
        }

        // Auto-fill redeem script for P2SH inputs.
        if (psgt.inputs[i].redeem_script.empty() && !psgt.inputs[i].non_witness_utxo.IsNull())
        {
            const COutPoint& prevout = psgt.tx.vin[i].prevout;
            if (prevout.n < psgt.inputs[i].non_witness_utxo.vout.size())
            {
                const CScript& scriptPubKey = psgt.inputs[i].non_witness_utxo.vout[prevout.n].scriptPubKey;
                if (scriptPubKey.IsPayToScriptHash())
                {
                    CScriptID scriptID(uint160(std::vector<unsigned char>(
                        scriptPubKey.begin() + 2, scriptPubKey.begin() + 22)));
                    CScript redeemScript;
                    if (pwalletMain->GetCScript(scriptID, redeemScript))
                        psgt.inputs[i].redeem_script = redeemScript;
                }
            }
        }
    }

    // Sign each input with wallet keys.
    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        SignPSGTInput(*pwalletMain, psgt, i, nHashType);
    }

    // Determine completeness by attempting finalization on a copy.
    PartiallySignedTransaction psgt_copy = psgt;
    bool complete = FinalizePSGT(psgt_copy);

    // Populate HD keypaths for inputs.
    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        if (!psgt.inputs[i].non_witness_utxo.IsNull())
        {
            const COutPoint& prevout = psgt.tx.vin[i].prevout;
            if (prevout.n < psgt.inputs[i].non_witness_utxo.vout.size())
                FillHDKeypaths(*pwalletMain,
                               psgt.inputs[i].non_witness_utxo.vout[prevout.n].scriptPubKey,
                               psgt.inputs[i].hd_keypaths);
        }
    }

    // Update outputs (redeem scripts + HD keypaths).
    for (unsigned int i = 0; i < psgt.outputs.size(); ++i)
    {
        UpdatePSGTOutput(*pwalletMain, psgt, i);
        FillHDKeypaths(*pwalletMain,
                       psgt.tx.vout[i].scriptPubKey,
                       psgt.outputs[i].hd_keypaths);
    }

    UniValue result(UniValue::VOBJ);
    vector<unsigned char> data = SerializePSGT(psgt);
    result.pushKV("psgt", EncodeBase64(data.data(), data.size()));
    result.pushKV("complete", complete);

    return result;
}

UniValue utxoupdatepsgt(const UniValue& params, bool fHelp)
{
    static const RPCHelpMan help{
        "utxoupdatepsgt",
        "Update a PSGT with UTXO data from the node. "
        "For each input where non_witness_utxo is missing, look up the previous transaction and fill it in.",
        {
            {"psgt", RPCArg::Type::STR, RPCArg::Optional::NO,
                "A base64-encoded PSGT."},
        },
        RPCResult{RPCResult::Type::STR, "psgt", "The base64-encoded updated PSGT."},
        RPCExamples{
            HelpExampleCli("utxoupdatepsgt", "\"cHNidP8B...\"") +
            HelpExampleRpc("utxoupdatepsgt", "\"cHNidP8B...\"")},
    };
    if (fHelp || !help.IsValidNumArgs(params.size()))
        throw runtime_error(help.ToString());

    PartiallySignedTransaction psgt;
    string error;
    if (!DecodeRawPSGT(psgt, params[0].get_str(), error))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, error);

    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        if (psgt.inputs[i].non_witness_utxo.IsNull())
        {
            const uint256& prevHash = psgt.tx.vin[i].prevout.hash;
            CTransaction prevTx;
            uint256 hashBlock;
            if (GetTransaction(prevHash, prevTx, hashBlock))
                psgt.inputs[i].non_witness_utxo = prevTx;
        }
    }

    vector<unsigned char> data = SerializePSGT(psgt);
    return EncodeBase64(data.data(), data.size());
}

UniValue converttopsgt(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "converttopsgt \"hexstring\"\n"
            "\nConvert a raw transaction to PSGT format.\n"
            "The resulting PSGT will have empty input metadata; use\n"
            "utxoupdatepsgt to fill in UTXO information.\n"
            "\nArguments:\n"
            "1. \"hexstring\"        (string, required) The hex-encoded raw transaction\n"
            "\nResult:\n"
            "  \"psgt\"              (string) The base64-encoded PSGT\n"
        );

    vector<unsigned char> txData(ParseHex(params[0].get_str()));
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    try {
        ssData >> tx;
    } catch (const std::exception& e) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    CMutableTransaction mtx(tx);

    // Store any existing scriptSigs, then clear them for the unsigned tx.
    vector<CScript> savedScriptSigs;
    for (auto& vin : mtx.vin)
    {
        savedScriptSigs.push_back(vin.scriptSig);
        vin.scriptSig.clear();
    }

    PartiallySignedTransaction psgt(mtx);

    // If any inputs had scriptSigs, store them as final_script_sig.
    for (unsigned int i = 0; i < savedScriptSigs.size(); ++i)
    {
        if (!savedScriptSigs[i].empty())
            psgt.inputs[i].final_script_sig = savedScriptSigs[i];
    }

    vector<unsigned char> data = SerializePSGT(psgt);
    return EncodeBase64(data.data(), data.size());
}

UniValue walletcreatefundedpsgt(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "walletcreatefundedpsgt [{\"txid\":\"id\",\"vout\":n},...] {\"address\":amount,...} ( options sign )\n"
            "\nCreate and fund a PSGT with wallet UTXOs, optionally signing it.\n"
            "\nArguments:\n"
            "1. \"inputs\"           (array, required) A json array of json objects (can be empty for auto-selection)\n"
            "2. \"outputs\"          (object, required) a json object with addresses as keys and amounts as values\n"
            "3. \"options\"          (object, optional)\n"
            "    {\n"
            "      \"changeAddress\" : \"addr\"   (string, optional) Address for change output\n"
            "    }\n"
            "4. sign                 (boolean, optional, default true) Also sign the PSGT with wallet keys\n"
            "\nResult:\n"
            "{\n"
            "  \"psgt\"       : \"value\",   (string) The base64-encoded PSGT\n"
            "  \"fee\"        : n            (numeric) The fee paid\n"
            "}\n"
        );

    RPCTypeCheck(params, {UniValue::VARR, UniValue::VOBJ});

    EnsureWalletIsUnlocked();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Build the raw transaction first via createpsgt logic.
    UniValue inputs = params[0].get_array();
    UniValue outputs = params[1].get_obj();

    CMutableTransaction mtx;
    mtx.nVersion = CTransaction::CURRENT_VERSION;

    for (unsigned int idx = 0; idx < inputs.size(); idx++)
    {
        const UniValue& input = inputs[idx];
        const UniValue& o = input.get_obj();

        uint256 txid;
        txid.SetHex(find_value(o, "txid").get_str());
        int nOutput = find_value(o, "vout").get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        mtx.vin.push_back(CTxIn(COutPoint(txid, nOutput)));
    }

    vector<string> addrList = outputs.getKeys();
    for (const string& name_ : addrList)
    {
        CTxDestination dest = DecodeDestination(name_);
        if (!IsValidDestination(dest))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Gridcoin address: ") + name_);

        CAmount nAmount = AmountFromValue(outputs[name_]);
        CScript scriptPubKey;
        scriptPubKey.SetDestination(dest);
        mtx.vout.push_back(CTxOut(nAmount, scriptPubKey));
    }

    // Build the send vector for CreateTransaction.
    vector<pair<CScript, int64_t>> vecSend;
    for (const auto& txout : mtx.vout)
        vecSend.push_back({txout.scriptPubKey, txout.nValue});

    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired = 0;
    string strError;

    CCoinControl coinControl;

    if (params.size() > 2 && !params[2].isNull())
    {
        UniValue options = params[2].get_obj();
        if (options.exists("changeAddress"))
        {
            CTxDestination changeDest = DecodeDestination(options["changeAddress"].get_str());
            if (!IsValidDestination(changeDest))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid change address");
            coinControl.destChange = changeDest;
        }
    }

    // Use CreateTransaction to fund it.
    CWalletTx wtxNew;
    if (!pwalletMain->CreateTransaction(vecSend, wtxNew, reservekey,
                                         nFeeRequired, &coinControl))
    {
        throw JSONRPCError(RPC_WALLET_ERROR, "CreateTransaction failed");
    }

    CMutableTransaction fundedTx(static_cast<const CTransaction&>(wtxNew));

    // Clear scriptSigs for unsigned PSGT.
    for (auto& vin : fundedTx.vin)
        vin.scriptSig.clear();

    PartiallySignedTransaction psgt(fundedTx);

    // Optionally sign.
    bool fSign = true;
    if (params.size() > 3)
        fSign = params[3].get_bool();

    bool complete = false;
    if (fSign)
    {
        // Fill UTXO data.
        for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
        {
            const uint256& prevHash = psgt.tx.vin[i].prevout.hash;
            CTransaction prevTx;
            uint256 hashBlock;
            if (GetTransaction(prevHash, prevTx, hashBlock))
                psgt.inputs[i].non_witness_utxo = prevTx;
        }

        complete = true;
        for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
        {
            if (!SignPSGTInput(*pwalletMain, psgt, i))
                complete = false;
        }
    }

    // Update outputs (redeem scripts + HD keypaths).
    for (unsigned int i = 0; i < psgt.outputs.size(); ++i)
    {
        UpdatePSGTOutput(*pwalletMain, psgt, i);
        FillHDKeypaths(*pwalletMain,
                       psgt.tx.vout[i].scriptPubKey,
                       psgt.outputs[i].hd_keypaths);
    }

    UniValue result(UniValue::VOBJ);
    vector<unsigned char> data = SerializePSGT(psgt);
    result.pushKV("psgt", EncodeBase64(data.data(), data.size()));
    result.pushKV("fee", ValueFromAmount(nFeeRequired));

    return result;
}
