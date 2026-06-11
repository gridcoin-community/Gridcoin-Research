// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <psgt.h>

#include <hash.h>
#include <keystore.h>
#include <policy/fees.h>
#include <script/interpreter.h>
#include <script/sign.h>
#include <script/standard.h>
#include <span.h>
#include <streams.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <version.h>

#include <algorithm>
#include <cassert>
#include <set>

// Helper: extract a vector<unsigned char> from a CDataStream.
// CDataStream iterates over std::byte, not unsigned char, so we
// need to cast via the raw data pointer.
static std::vector<unsigned char> StreamToUCharVec(const CDataStream& ss)
{
    const unsigned char* p = UCharCast(ss.data());
    return std::vector<unsigned char>(p, p + ss.size());
}

PartiallySignedTransaction::PartiallySignedTransaction(const CMutableTransaction& txIn)
    : tx(txIn)
    , inputs(txIn.vin.size())
    , outputs(txIn.vout.size())
{
}

bool PSGTInputSigned(const PSGTInput& input)
{
    return !input.final_script_sig.empty();
}

bool SignPSGTInput(const SigningProvider& provider,
                   PartiallySignedTransaction& psgt,
                   unsigned int index,
                   int sighash)
{
    if (index >= psgt.inputs.size() || index >= psgt.tx.vin.size())
        return false;

    PSGTInput& input = psgt.inputs[index];

    // Already finalized — nothing to do.
    if (PSGTInputSigned(input))
        return true;

    // We need the previous output's scriptPubKey.
    // It must be provided via the non_witness_utxo.
    if (input.non_witness_utxo.IsNull())
        return false;

    const COutPoint& prevout = psgt.tx.vin[index].prevout;
    if (prevout.n >= input.non_witness_utxo.vout.size())
        return false;

    const CScript& scriptPubKey = input.non_witness_utxo.vout[prevout.n].scriptPubKey;

    // Determine the script we need to sign. For P2SH, unwrap to the
    // redeem script; for bare scripts, sign the scriptPubKey directly.
    CScript signScript = scriptPubKey;
    txnouttype scriptType;
    std::vector<std::vector<unsigned char>> vSolutions;

    if (scriptPubKey.IsPayToScriptHash())
    {
        // P2SH: we need the redeem script.
        if (input.redeem_script.empty())
        {
            // Try to look it up from the provider.
            CScriptID scriptID(uint160(std::vector<unsigned char>(
                scriptPubKey.begin() + 2, scriptPubKey.begin() + 22)));
            CScript redeemScript;
            if (provider.GetCScript(scriptID, redeemScript))
                input.redeem_script = redeemScript;
            else
                return false;
        }
        signScript = input.redeem_script;
    }

    Solver(signScript, scriptType, vSolutions);

    MutableTransactionSignatureCreator creator(psgt.tx, index, sighash);

    if (scriptType == TX_MULTISIG)
    {
        // Multisig: accumulate individual signatures in partial_sigs.
        // vSolutions[0] = M, vSolutions[1..N] = pubkeys, vSolutions[N+1] = N
        bool added_any = false;
        for (unsigned int i = 1; i < vSolutions.size() - 1; ++i)
        {
            CPubKey pubkey(vSolutions[i]);
            CKeyID keyid = pubkey.GetID();

            // Skip if we already have a signature for this key.
            if (input.partial_sigs.count(keyid))
                continue;

            std::vector<unsigned char> vchSig;
            if (creator.CreateSig(provider, vchSig, keyid, signScript))
            {
                input.partial_sigs[keyid] = vchSig;
                added_any = true;
            }
        }
        return added_any || !input.partial_sigs.empty();
    }
    else
    {
        // P2PKH, P2PK, or other: ProduceSignature produces a complete scriptSig.
        SignatureData sigdata;
        if (!ProduceSignature(provider, creator, scriptPubKey, sigdata))
            return false;

        input.final_script_sig = sigdata.scriptSig;
        return true;
    }
}

/** Try to finalize a single input by assembling partial_sigs into final_script_sig. */
static bool FinalizeInput(PSGTInput& input, const CScript& scriptPubKey)
{
    // Already finalized.
    if (!input.final_script_sig.empty())
        return true;

    // Determine the script to finalize against.
    CScript signScript = scriptPubKey;
    bool is_p2sh = false;

    if (scriptPubKey.IsPayToScriptHash() && !input.redeem_script.empty())
    {
        signScript = input.redeem_script;
        is_p2sh = true;
    }

    txnouttype scriptType;
    std::vector<std::vector<unsigned char>> vSolutions;
    Solver(signScript, scriptType, vSolutions);

    CScript result;

    if (scriptType == TX_MULTISIG)
    {
        int nRequired = vSolutions.front()[0];
        int nSigsHave = 0;

        result << OP_0; // CHECKMULTISIG bug workaround

        // Add signatures in pubkey order.
        for (unsigned int i = 1; i < vSolutions.size() - 1 && nSigsHave < nRequired; ++i)
        {
            CPubKey pubkey(vSolutions[i]);
            CKeyID keyid = pubkey.GetID();

            auto it = input.partial_sigs.find(keyid);
            if (it != input.partial_sigs.end())
            {
                result << it->second;
                ++nSigsHave;
            }
        }

        if (nSigsHave < nRequired)
            return false;
    }
    else if (scriptType == TX_PUBKEYHASH)
    {
        // Need exactly one signature for the pubkeyhash.
        if (input.partial_sigs.size() != 1)
            return false;

        const auto& sig_entry = *input.partial_sigs.begin();
        result << sig_entry.second;
        // We also need the pubkey — but partial_sigs is keyed by CKeyID,
        // and we don't store the full pubkey. For P2PKH, the signing path
        // goes through ProduceSignature which sets final_script_sig directly.
        // If we reach here with partial_sigs for P2PKH, something is wrong.
        return false;
    }
    else
    {
        // Other script types: nothing to assemble from partial_sigs.
        return false;
    }

    // For P2SH, append the serialized redeem script.
    if (is_p2sh)
    {
        result << std::vector<unsigned char>(input.redeem_script.begin(),
                                             input.redeem_script.end());
    }

    input.final_script_sig = result;
    return true;
}

bool FinalizePSGT(PartiallySignedTransaction& psgt)
{
    bool complete = true;
    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        PSGTInput& input = psgt.inputs[i];

        // Already finalized?
        if (!input.final_script_sig.empty())
            continue;

        // Need the previous output's scriptPubKey.
        if (input.non_witness_utxo.IsNull())
        {
            complete = false;
            continue;
        }

        const COutPoint& prevout = psgt.tx.vin[i].prevout;
        if (prevout.n >= input.non_witness_utxo.vout.size())
        {
            complete = false;
            continue;
        }

        const CScript& scriptPubKey = input.non_witness_utxo.vout[prevout.n].scriptPubKey;

        if (!FinalizeInput(input, scriptPubKey))
            complete = false;
    }
    return complete;
}

bool FinalizeAndExtractPSGT(PartiallySignedTransaction& psgt, CMutableTransaction& result)
{
    if (!FinalizePSGT(psgt))
        return false;

    result = psgt.tx;
    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        result.vin[i].scriptSig = psgt.inputs[i].final_script_sig;
    }

    return true;
}

bool CombinePSGTs(PartiallySignedTransaction& out,
                   const std::vector<PartiallySignedTransaction>& psgts)
{
    if (psgts.empty())
        return false;

    // All PSGTs must have the same unsigned transaction.
    const uint256 txhash = psgts[0].tx.GetHash();
    for (size_t i = 1; i < psgts.size(); ++i)
    {
        if (psgts[i].tx.GetHash() != txhash)
            return false;
    }

    out = psgts[0];

    // Merge inputs from all PSGTs.
    for (size_t i = 1; i < psgts.size(); ++i)
    {
        for (size_t j = 0; j < out.inputs.size() && j < psgts[i].inputs.size(); ++j)
        {
            const PSGTInput& src = psgts[i].inputs[j];

            // Take non_witness_utxo if we don't have one.
            if (out.inputs[j].non_witness_utxo.IsNull() && !src.non_witness_utxo.IsNull())
                out.inputs[j].non_witness_utxo = src.non_witness_utxo;

            // Take redeem_script if we don't have one.
            if (out.inputs[j].redeem_script.empty() && !src.redeem_script.empty())
                out.inputs[j].redeem_script = src.redeem_script;

            // Merge partial signatures.
            for (const auto& sig : src.partial_sigs)
                out.inputs[j].partial_sigs.insert(sig);

            // Merge HD keypaths.
            for (const auto& kp : src.hd_keypaths)
                out.inputs[j].hd_keypaths.insert(kp);

            // Merge unknown fields.
            for (const auto& u : src.unknown)
                out.inputs[j].unknown.insert(u);

            // Take final_script_sig if we don't have one.
            if (out.inputs[j].final_script_sig.empty() && !src.final_script_sig.empty())
                out.inputs[j].final_script_sig = src.final_script_sig;
        }

        // Merge outputs.
        for (size_t j = 0; j < out.outputs.size() && j < psgts[i].outputs.size(); ++j)
        {
            const PSGTOutput& src = psgts[i].outputs[j];

            if (out.outputs[j].redeem_script.empty() && !src.redeem_script.empty())
                out.outputs[j].redeem_script = src.redeem_script;

            for (const auto& kp : src.hd_keypaths)
                out.outputs[j].hd_keypaths.insert(kp);

            for (const auto& u : src.unknown)
                out.outputs[j].unknown.insert(u);
        }
    }

    return true;
}

void UpdatePSGTOutput(const SigningProvider& provider,
                       PartiallySignedTransaction& psgt,
                       unsigned int index)
{
    if (index >= psgt.outputs.size() || index >= psgt.tx.vout.size())
        return;

    PSGTOutput& output = psgt.outputs[index];
    const CScript& scriptPubKey = psgt.tx.vout[index].scriptPubKey;

    // For P2SH outputs, look up the redeem script.
    if (output.redeem_script.empty() && scriptPubKey.IsPayToScriptHash())
    {
        CScriptID scriptID(uint160(std::vector<unsigned char>(
            scriptPubKey.begin() + 2, scriptPubKey.begin() + 22)));
        CScript redeemScript;
        if (provider.GetCScript(scriptID, redeemScript))
            output.redeem_script = redeemScript;
    }
}

// --- BIP 174-style binary serialization ---

// Helper: write a compact size to a byte vector.
static void WriteCompactSize(std::vector<unsigned char>& stream, uint64_t nSize)
{
    if (nSize < 253) {
        stream.push_back((unsigned char)nSize);
    } else if (nSize <= 0xffffU) {
        stream.push_back(253);
        stream.push_back((unsigned char)(nSize & 0xff));
        stream.push_back((unsigned char)((nSize >> 8) & 0xff));
    } else if (nSize <= 0xffffffffU) {
        stream.push_back(254);
        for (int i = 0; i < 4; ++i)
            stream.push_back((unsigned char)((nSize >> (8 * i)) & 0xff));
    } else {
        stream.push_back(255);
        for (int i = 0; i < 8; ++i)
            stream.push_back((unsigned char)((nSize >> (8 * i)) & 0xff));
    }
}

// Helper: serialize a key-value pair (key = type byte + optional data, value = data).
static void SerializeKeyValue(std::vector<unsigned char>& stream,
                               const std::vector<unsigned char>& key,
                               const std::vector<unsigned char>& value)
{
    WriteCompactSize(stream, key.size());
    stream.insert(stream.end(), key.begin(), key.end());
    WriteCompactSize(stream, value.size());
    stream.insert(stream.end(), value.begin(), value.end());
}

std::vector<unsigned char> SerializePSGT(const PartiallySignedTransaction& psgt)
{
    std::vector<unsigned char> result;

    // Magic bytes.
    result.insert(result.end(), PSGT_MAGIC.begin(), PSGT_MAGIC.end());

    // Global: unsigned transaction.
    {
        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << psgt.tx;
        std::vector<unsigned char> txData = StreamToUCharVec(ssTx);

        std::vector<unsigned char> key = {PSGT_GLOBAL_UNSIGNED_TX};
        SerializeKeyValue(result, key, txData);
    }

    // Global: unknown fields.
    for (const auto& entry : psgt.unknown)
    {
        SerializeKeyValue(result, entry.first, entry.second);
    }

    // Global separator.
    result.push_back(0x00);

    // Per-input maps.
    for (const auto& input : psgt.inputs)
    {
        // Non-witness UTXO.
        if (!input.non_witness_utxo.IsNull())
        {
            CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
            ss << input.non_witness_utxo;
            std::vector<unsigned char> key = {PSGT_IN_NON_WITNESS_UTXO};
            std::vector<unsigned char> val = StreamToUCharVec(ss);
            SerializeKeyValue(result, key, val);
        }

        // Partial signatures.
        for (const auto& sig : input.partial_sigs)
        {
            std::vector<unsigned char> key;
            key.push_back(PSGT_IN_PARTIAL_SIG);
            // Key includes the pubkey for this keyid — we store just the keyid bytes.
            const auto& keyid_bytes = sig.first;
            key.insert(key.end(), keyid_bytes.begin(), keyid_bytes.end());
            SerializeKeyValue(result, key, sig.second);
        }

        // Sighash type.
        if (input.sighash_type != 0)
        {
            std::vector<unsigned char> key = {PSGT_IN_SIGHASH_TYPE};
            std::vector<unsigned char> val(4);
            val[0] = (unsigned char)(input.sighash_type & 0xff);
            val[1] = (unsigned char)((input.sighash_type >> 8) & 0xff);
            val[2] = (unsigned char)((input.sighash_type >> 16) & 0xff);
            val[3] = (unsigned char)((input.sighash_type >> 24) & 0xff);
            SerializeKeyValue(result, key, val);
        }

        // Redeem script.
        if (!input.redeem_script.empty())
        {
            std::vector<unsigned char> key = {PSGT_IN_REDEEM_SCRIPT};
            std::vector<unsigned char> val(input.redeem_script.begin(), input.redeem_script.end());
            SerializeKeyValue(result, key, val);
        }

        // BIP32 derivation paths.
        for (const auto& kp : input.hd_keypaths)
        {
            std::vector<unsigned char> key;
            key.push_back(PSGT_IN_BIP32_DERIVATION);
            auto pubkey_data = kp.first.IsValid()
                ? std::vector<unsigned char>(kp.first.begin(), kp.first.end())
                : std::vector<unsigned char>();
            key.insert(key.end(), pubkey_data.begin(), pubkey_data.end());

            std::vector<unsigned char> val;
            val.insert(val.end(), kp.second.fingerprint, kp.second.fingerprint + 4);
            for (uint32_t idx : kp.second.path)
            {
                val.push_back((unsigned char)(idx & 0xff));
                val.push_back((unsigned char)((idx >> 8) & 0xff));
                val.push_back((unsigned char)((idx >> 16) & 0xff));
                val.push_back((unsigned char)((idx >> 24) & 0xff));
            }
            SerializeKeyValue(result, key, val);
        }

        // Final scriptSig.
        if (!input.final_script_sig.empty())
        {
            std::vector<unsigned char> key = {PSGT_IN_FINAL_SCRIPTSIG};
            std::vector<unsigned char> val(input.final_script_sig.begin(), input.final_script_sig.end());
            SerializeKeyValue(result, key, val);
        }

        // Unknown fields.
        for (const auto& entry : input.unknown)
        {
            SerializeKeyValue(result, entry.first, entry.second);
        }

        // Input separator.
        result.push_back(0x00);
    }

    // Per-output maps.
    for (const auto& output : psgt.outputs)
    {
        // Redeem script.
        if (!output.redeem_script.empty())
        {
            std::vector<unsigned char> key = {PSGT_OUT_REDEEM_SCRIPT};
            std::vector<unsigned char> val(output.redeem_script.begin(), output.redeem_script.end());
            SerializeKeyValue(result, key, val);
        }

        // BIP32 derivation paths.
        for (const auto& kp : output.hd_keypaths)
        {
            std::vector<unsigned char> key;
            key.push_back(PSGT_OUT_BIP32_DERIVATION);
            auto pubkey_data = kp.first.IsValid()
                ? std::vector<unsigned char>(kp.first.begin(), kp.first.end())
                : std::vector<unsigned char>();
            key.insert(key.end(), pubkey_data.begin(), pubkey_data.end());

            std::vector<unsigned char> val;
            val.insert(val.end(), kp.second.fingerprint, kp.second.fingerprint + 4);
            for (uint32_t idx : kp.second.path)
            {
                val.push_back((unsigned char)(idx & 0xff));
                val.push_back((unsigned char)((idx >> 8) & 0xff));
                val.push_back((unsigned char)((idx >> 16) & 0xff));
                val.push_back((unsigned char)((idx >> 24) & 0xff));
            }
            SerializeKeyValue(result, key, val);
        }

        // Unknown fields.
        for (const auto& entry : output.unknown)
        {
            SerializeKeyValue(result, entry.first, entry.second);
        }

        // Output separator.
        result.push_back(0x00);
    }

    return result;
}

// Helper: read a compact size from a stream position.
static bool ReadCompactSizeFromVec(const std::vector<unsigned char>& data, size_t& pos, uint64_t& nSize)
{
    if (pos >= data.size()) return false;

    unsigned char chSize = data[pos++];
    if (chSize < 253)
    {
        nSize = chSize;
    }
    else if (chSize == 253)
    {
        if (pos + 2 > data.size()) return false;
        nSize = data[pos] | ((uint64_t)data[pos + 1] << 8);
        pos += 2;
    }
    else if (chSize == 254)
    {
        if (pos + 4 > data.size()) return false;
        nSize = 0;
        for (int i = 0; i < 4; ++i)
            nSize |= ((uint64_t)data[pos + i] << (8 * i));
        pos += 4;
    }
    else
    {
        if (pos + 8 > data.size()) return false;
        nSize = 0;
        for (int i = 0; i < 8; ++i)
            nSize |= ((uint64_t)data[pos + i] << (8 * i));
        pos += 8;
    }
    return true;
}

// Helper: read N bytes from a vector at a given position.
static bool ReadBytes(const std::vector<unsigned char>& data, size_t& pos,
                      std::vector<unsigned char>& out, size_t n)
{
    if (pos + n > data.size()) return false;
    out.assign(data.begin() + pos, data.begin() + pos + n);
    pos += n;
    return true;
}

bool DecodeRawPSGT(PartiallySignedTransaction& psgt,
                    const std::string& base64_tx,
                    std::string& error)
{
    // Strip whitespace (spaces, tabs, newlines) that may be introduced
    // by terminal line-wrapping or copy-paste.
    std::string cleaned;
    cleaned.reserve(base64_tx.size());
    for (char c : base64_tx)
    {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
            cleaned += c;
    }

    bool invalid = false;
    std::vector<unsigned char> data = DecodeBase64(cleaned.c_str(), &invalid);
    if (invalid)
    {
        error = "Invalid base64";
        return false;
    }

    // Check magic bytes.
    if (data.size() < PSGT_MAGIC.size() ||
        !std::equal(PSGT_MAGIC.begin(), PSGT_MAGIC.end(), data.begin()))
    {
        error = "Invalid PSGT magic bytes";
        return false;
    }

    size_t pos = PSGT_MAGIC.size();

    // Parse global map.
    bool found_tx = false;
    std::set<std::vector<unsigned char>> global_keys;
    while (pos < data.size())
    {
        // Check for separator.
        uint64_t keyLen = 0;
        size_t savedPos = pos;
        if (!ReadCompactSizeFromVec(data, pos, keyLen)) { error = "Truncated global key length"; return false; }
        if (keyLen == 0) break; // Separator

        std::vector<unsigned char> key;
        if (!ReadBytes(data, pos, key, keyLen)) { error = "Truncated global key"; return false; }

        uint64_t valLen = 0;
        if (!ReadCompactSizeFromVec(data, pos, valLen)) { error = "Truncated global value length"; return false; }
        std::vector<unsigned char> val;
        if (!ReadBytes(data, pos, val, valLen)) { error = "Truncated global value"; return false; }

        if (!global_keys.insert(key).second) { error = "Duplicate key in global map"; return false; }

        if (key.size() >= 1 && key[0] == PSGT_GLOBAL_UNSIGNED_TX && !found_tx)
        {
            CDataStream ssTx(val, SER_NETWORK, PROTOCOL_VERSION);
            try {
                ssTx >> psgt.tx;
            } catch (const std::exception& e) {
                error = std::string("TX decode failed: ") + e.what();
                return false;
            }
            found_tx = true;

            // Ensure the transaction inputs have no scriptSigs (unsigned).
            for (const auto& vin : psgt.tx.vin)
            {
                if (!vin.scriptSig.empty())
                {
                    error = "Unsigned tx has non-empty scriptSig";
                    return false;
                }
            }
        }
        else
        {
            psgt.unknown[key] = val;
        }
    }

    if (!found_tx)
    {
        error = "No unsigned transaction found in PSGT";
        return false;
    }

    // Initialize input/output vectors.
    psgt.inputs.resize(psgt.tx.vin.size());
    psgt.outputs.resize(psgt.tx.vout.size());

    // Parse per-input maps.
    for (unsigned int i = 0; i < psgt.tx.vin.size(); ++i)
    {
        std::set<std::vector<unsigned char>> input_keys;
        while (pos < data.size())
        {
            uint64_t keyLen = 0;
            if (!ReadCompactSizeFromVec(data, pos, keyLen)) { error = "Truncated input key length"; return false; }
            if (keyLen == 0) break; // Separator

            std::vector<unsigned char> key;
            if (!ReadBytes(data, pos, key, keyLen)) { error = "Truncated input key"; return false; }

            uint64_t valLen = 0;
            if (!ReadCompactSizeFromVec(data, pos, valLen)) { error = "Truncated input value length"; return false; }
            std::vector<unsigned char> val;
            if (!ReadBytes(data, pos, val, valLen)) { error = "Truncated input value"; return false; }

            if (!input_keys.insert(key).second) { error = "Duplicate key in input map"; return false; }

            uint8_t type = key[0];
            switch (type)
            {
            case PSGT_IN_NON_WITNESS_UTXO:
            {
                CDataStream ss(val, SER_NETWORK, PROTOCOL_VERSION);
                try { ss >> psgt.inputs[i].non_witness_utxo; }
                catch (...) { error = "Failed to decode non-witness UTXO"; return false; }
                break;
            }
            case PSGT_IN_PARTIAL_SIG:
            {
                if (key.size() < 2) { error = "Partial sig key too short"; return false; }
                CKeyID keyid;
                memcpy(keyid.begin(), key.data() + 1, std::min(key.size() - 1, (size_t)20));
                psgt.inputs[i].partial_sigs[keyid] = val;
                break;
            }
            case PSGT_IN_SIGHASH_TYPE:
            {
                if (val.size() != 4) { error = "Invalid sighash type length"; return false; }
                psgt.inputs[i].sighash_type = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24);
                break;
            }
            case PSGT_IN_REDEEM_SCRIPT:
                psgt.inputs[i].redeem_script = CScript(val.begin(), val.end());
                break;
            case PSGT_IN_FINAL_SCRIPTSIG:
                psgt.inputs[i].final_script_sig = CScript(val.begin(), val.end());
                break;
            case PSGT_IN_BIP32_DERIVATION:
            {
                // Key: type byte + compressed pubkey (33 bytes)
                if (key.size() < 34) { error = "BIP32 derivation key too short"; return false; }
                CPubKey pubkey(key.begin() + 1, key.end());
                if (!pubkey.IsValid()) { error = "Invalid pubkey in BIP32 derivation"; return false; }
                // Value: 4-byte fingerprint + 4 bytes per path element
                if (val.size() < 4 || (val.size() - 4) % 4 != 0) { error = "Invalid BIP32 derivation value"; return false; }
                KeyOriginInfo info;
                memcpy(info.fingerprint, val.data(), 4);
                for (size_t j = 4; j < val.size(); j += 4)
                {
                    uint32_t idx = val[j] | (uint32_t(val[j+1]) << 8) |
                                   (uint32_t(val[j+2]) << 16) | (uint32_t(val[j+3]) << 24);
                    info.path.push_back(idx);
                }
                psgt.inputs[i].hd_keypaths[pubkey] = info;
                break;
            }
            default:
                psgt.inputs[i].unknown[key] = val;
                break;
            }
        }
    }

    // Parse per-output maps.
    for (unsigned int i = 0; i < psgt.tx.vout.size(); ++i)
    {
        std::set<std::vector<unsigned char>> output_keys;
        while (pos < data.size())
        {
            uint64_t keyLen = 0;
            if (!ReadCompactSizeFromVec(data, pos, keyLen)) { error = "Truncated output key length"; return false; }
            if (keyLen == 0) break; // Separator

            std::vector<unsigned char> key;
            if (!ReadBytes(data, pos, key, keyLen)) { error = "Truncated output key"; return false; }

            uint64_t valLen = 0;
            if (!ReadCompactSizeFromVec(data, pos, valLen)) { error = "Truncated output value length"; return false; }
            std::vector<unsigned char> val;
            if (!ReadBytes(data, pos, val, valLen)) { error = "Truncated output value"; return false; }

            if (!output_keys.insert(key).second) { error = "Duplicate key in output map"; return false; }

            uint8_t type = key[0];
            switch (type)
            {
            case PSGT_OUT_REDEEM_SCRIPT:
                psgt.outputs[i].redeem_script = CScript(val.begin(), val.end());
                break;
            case PSGT_OUT_BIP32_DERIVATION:
            {
                if (key.size() < 34) { error = "BIP32 derivation key too short"; return false; }
                CPubKey pubkey(key.begin() + 1, key.end());
                if (!pubkey.IsValid()) { error = "Invalid pubkey in BIP32 derivation"; return false; }
                if (val.size() < 4 || (val.size() - 4) % 4 != 0) { error = "Invalid BIP32 derivation value"; return false; }
                KeyOriginInfo info;
                memcpy(info.fingerprint, val.data(), 4);
                for (size_t j = 4; j < val.size(); j += 4)
                {
                    uint32_t idx = val[j] | (uint32_t(val[j+1]) << 8) |
                                   (uint32_t(val[j+2]) << 16) | (uint32_t(val[j+3]) << 24);
                    info.path.push_back(idx);
                }
                psgt.outputs[i].hd_keypaths[pubkey] = info;
                break;
            }
            default:
                psgt.outputs[i].unknown[key] = val;
                break;
            }
        }
    }

    return true;
}

std::string PSGTRoleName(PSGTRole role)
{
    switch (role) {
    case PSGTRole::CREATOR: return "creator";
    case PSGTRole::UPDATER: return "updater";
    case PSGTRole::SIGNER: return "signer";
    case PSGTRole::FINALIZER: return "finalizer";
    case PSGTRole::EXTRACTOR: return "extractor";
    } // no default case, so the compiler can warn about missing cases
    assert(false);
    return "";
}

// Maximum serialized scriptSig sizes used for final-size estimation.
// A DER-encoded ECDSA signature plus sighash byte is at most 72 bytes,
// 73 with its push opcode; a compressed pubkey is 33 bytes, 34 pushed.
static constexpr unsigned int DUMMY_SIG_PUSH_SIZE = 73;
static constexpr unsigned int DUMMY_PUBKEY_PUSH_SIZE = 34;

/** Serialized size of pushing a redeem script onto a scriptSig. */
static unsigned int RedeemScriptPushSize(const CScript& redeem_script)
{
    const size_t n = redeem_script.size();
    return n + (n < OP_PUSHDATA1 ? 1 : n <= 0xff ? 2 : 3);
}

PSGTAnalysis AnalyzePSGT(const PartiallySignedTransaction& psgtx)
{
    PSGTAnalysis result;

    if (psgtx.tx.vin.empty()) {
        result.next = PSGTRole::CREATOR;
        return result;
    }

    if (psgtx.inputs.size() != psgtx.tx.vin.size()) {
        result.error = "PSGT input count does not match unsigned transaction input count";
        result.next = PSGTRole::CREATOR;
        return result;
    }

    bool all_have_utxo = true;
    bool all_sizable = true;
    CAmount in_amt = 0;

    // Copy of the unsigned tx whose scriptSigs are filled with dummies of the
    // expected final length, so one GetSerializeSize call yields the estimate.
    CMutableTransaction est_tx = psgtx.tx;

    result.inputs.resize(psgtx.inputs.size());

    for (unsigned int i = 0; i < psgtx.inputs.size(); ++i) {
        const PSGTInput& input = psgtx.inputs[i];
        PSGTInputAnalysis& ia = result.inputs[i];

        ia.is_final = PSGTInputSigned(input);

        // Unlike SignPSGTInput/FinalizePSGT, also require the provided
        // previous transaction to actually match prevout.hash — a mismatched
        // utxo would yield a bogus amount and scriptPubKey for analysis.
        const COutPoint& prevout = psgtx.tx.vin[i].prevout;
        ia.has_utxo = !input.non_witness_utxo.IsNull()
            && prevout.n < input.non_witness_utxo.vout.size()
            && input.non_witness_utxo.GetHash() == prevout.hash;

        if (ia.has_utxo) {
            in_amt += input.non_witness_utxo.vout[prevout.n].nValue;
        } else {
            all_have_utxo = false;
        }

        if (ia.is_final) {
            ia.next = PSGTRole::EXTRACTOR;
            est_tx.vin[i].scriptSig = input.final_script_sig;
            continue;
        }

        if (!ia.has_utxo) {
            ia.next = PSGTRole::UPDATER;
            all_sizable = false;
            continue;
        }

        const CScript& scriptPubKey = input.non_witness_utxo.vout[prevout.n].scriptPubKey;
        CScript signScript = scriptPubKey;
        bool is_p2sh = scriptPubKey.IsPayToScriptHash();

        if (is_p2sh) {
            CScriptID expected(uint160(std::vector<unsigned char>(
                scriptPubKey.begin() + 2, scriptPubKey.begin() + 22)));
            if (input.redeem_script.empty() || CScriptID(input.redeem_script) != expected) {
                ia.missing_redeem_script = true;
                ia.next = PSGTRole::UPDATER;
                all_sizable = false;
                continue;
            }
            signScript = input.redeem_script;
        }

        txnouttype scriptType;
        std::vector<std::vector<unsigned char>> vSolutions;
        Solver(signScript, scriptType, vSolutions);

        unsigned int script_sig_size = 0;

        switch (scriptType) {
        case TX_PUBKEY:
        {
            ia.missing_sigs.push_back(CPubKey(vSolutions[0]).GetID());
            ia.next = PSGTRole::SIGNER;
            script_sig_size = DUMMY_SIG_PUSH_SIZE;
            break;
        }
        case TX_PUBKEYHASH:
        {
            const CKeyID keyid = CKeyID(uint160(vSolutions[0]));
            ia.missing_sigs.push_back(keyid);
            // The signer also needs the full pubkey; report it missing unless
            // the PSGT carries it in hd_keypaths (a wallet signer will have
            // its own copy, but an offline analyzer cannot know that).
            bool have_pubkey = false;
            for (const auto& kp : input.hd_keypaths) {
                if (kp.first.GetID() == keyid) {
                    have_pubkey = true;
                    break;
                }
            }
            if (!have_pubkey) ia.missing_pubkeys.push_back(keyid);
            ia.next = PSGTRole::SIGNER;
            script_sig_size = DUMMY_SIG_PUSH_SIZE + DUMMY_PUBKEY_PUSH_SIZE;
            break;
        }
        case TX_MULTISIG:
        {
            // vSolutions[0][0] = required count, [1..N] = pubkeys, [N+1][0] = key count
            const unsigned int required = vSolutions.front()[0];
            unsigned int have = 0;
            for (unsigned int k = 1; k + 1 < vSolutions.size(); ++k) {
                CKeyID keyid = CPubKey(vSolutions[k]).GetID();
                if (input.partial_sigs.count(keyid)) {
                    ++have;
                } else {
                    ia.missing_sigs.push_back(keyid);
                }
            }
            if (have >= required) {
                // Enough signatures to assemble; the remaining keys are optional.
                ia.missing_sigs.clear();
                ia.next = PSGTRole::FINALIZER;
            } else {
                ia.next = PSGTRole::SIGNER;
            }
            script_sig_size = 1 /* OP_0 */ + required * DUMMY_SIG_PUSH_SIZE;
            break;
        }
        default:
            result.error = strprintf("Input %u spends a non-standard or unspendable output", i);
            ia.next = PSGTRole::UPDATER;
            all_sizable = false;
            continue;
        }

        if (is_p2sh) {
            script_sig_size += RedeemScriptPushSize(input.redeem_script);
        }

        // Raw filler bytes (not a push) so the dummy scriptSig serializes to
        // exactly script_sig_size bytes of script body.
        CScript dummy;
        dummy.insert(dummy.end(), script_sig_size, 0x00);
        est_tx.vin[i].scriptSig = dummy;
    }

    // Note: unlike Bitcoin, a non-final single-sig (P2PK/P2PKH) input is
    // always the signer's job, never the finalizer's — SignPSGTInput writes
    // final_script_sig directly and FinalizeInput cannot assemble single-sig
    // inputs from partial_sigs (the full pubkey is not stored there).
    result.next = PSGTRole::EXTRACTOR;
    for (const PSGTInputAnalysis& ia : result.inputs) {
        result.next = std::min(result.next, ia.next);
    }

    if (all_have_utxo) {
        CAmount out_amt = 0;
        for (const CTxOut& txout : psgtx.tx.vout) {
            out_amt += txout.nValue;
        }
        result.fee = in_amt - out_amt;
        if (*result.fee < 0) {
            result.error = "Transaction outputs exceed inputs";
        }
    }

    if (all_sizable) {
        const unsigned int est_size =
            ::GetSerializeSize(CTransaction(est_tx), SER_NETWORK, PROTOCOL_VERSION);
        result.estimated_final_size = est_size;
        result.min_required_fee =
            GetMinFee(CTransaction(psgtx.tx), 1000, GMF_SEND, est_size);
    }

    return result;
}
