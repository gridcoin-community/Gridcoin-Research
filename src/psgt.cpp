// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <psgt.h>

#include <hash.h>
#include <keystore.h>
#include <policy/fees.h>
#include <policy/policy.h>
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

// Read a length-prefixed byte string from a PSGT stream. ReadCompactSize bounds
// the length to MAX_SIZE and rejects non-canonical encodings; the additional
// remaining-bytes check bounds the allocation to the data actually present, so a
// small PSGT cannot claim a large length and force an oversized allocation
// before the read fails on truncation.
template <typename Stream>
static void ReadPSGTField(Stream& s, std::vector<unsigned char>& out)
{
    uint64_t len = ReadCompactSize(s);
    if (len > s.size())
        throw std::ios_base::failure("PSGT field length exceeds remaining data");
    out.resize(len);
    if (len > 0)
        s.read(AsWritableBytes(Span{out.data(), out.size()}));
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

    // Bound the input before decoding so a large base64 blob can't force a big
    // allocation ahead of the byte-level MAX_PSGT_WIRE_SIZE check. Base64 expands
    // 3 bytes -> 4 chars, so a valid PSGT encodes to at most this many characters.
    if (cleaned.size() > (MAX_PSGT_WIRE_SIZE / 3 + 1) * 4)
    {
        error = "PSGT exceeds maximum size";
        return false;
    }

    bool invalid = false;
    std::vector<unsigned char> data = DecodeBase64(cleaned.c_str(), &invalid);
    if (invalid)
    {
        error = "Invalid base64";
        return false;
    }

    return DecodePSGTBytes(psgt, data, error);
}

bool DecodePSGTBytes(PartiallySignedTransaction& psgt,
                     const std::vector<unsigned char>& data,
                     std::string& error)
{
    // Bound untrusted input before any work: a PSGT from an untrusted source
    // (the decode RPC/GUI, and the upcoming p2p relay) must not exceed the wire
    // cap. Together with the range-checked ReadCompactSize used by the stream
    // reads below, this removes attacker-controlled lengths as a crash vector.
    if (data.size() > MAX_PSGT_WIRE_SIZE)
    {
        error = "PSGT exceeds maximum size";
        return false;
    }

    // Check magic bytes.
    if (data.size() < PSGT_MAGIC.size() ||
        !std::equal(PSGT_MAGIC.begin(), PSGT_MAGIC.end(), data.begin()))
    {
        error = "Invalid PSGT magic bytes";
        return false;
    }

    // Parse the key/value maps through the standard serialization framework
    // instead of hand-rolled index walking: ReadPSGTField reads each
    // length-prefixed byte string via ReadCompactSize (which rejects
    // non-canonical and over-MAX_SIZE lengths) and rejects any length beyond the
    // remaining bytes before allocating -- so a malformed length can neither
    // overflow, over-read, nor force an oversized allocation.
    CDataStream ss(std::vector<unsigned char>(data.begin() + PSGT_MAGIC.size(), data.end()),
                   SER_NETWORK, PROTOCOL_VERSION);

    try {

    // Parse global map.
    bool found_tx = false;
    std::set<std::vector<unsigned char>> global_keys;
    while (!ss.empty())
    {
        std::vector<unsigned char> key;
        ReadPSGTField(ss, key);
        if (key.empty()) break; // Separator

        std::vector<unsigned char> val;
        ReadPSGTField(ss, val);

        if (!global_keys.insert(key).second) { error = "Duplicate key in global map"; return false; }

        if (key[0] == PSGT_GLOBAL_UNSIGNED_TX && !found_tx)
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
        while (!ss.empty())
        {
            std::vector<unsigned char> key;
            ReadPSGTField(ss, key);
            if (key.empty()) break; // Separator

            std::vector<unsigned char> val;
            ReadPSGTField(ss, val);

            if (!input_keys.insert(key).second) { error = "Duplicate key in input map"; return false; }

            uint8_t type = key[0];
            switch (type)
            {
            case PSGT_IN_NON_WITNESS_UTXO:
            {
                CDataStream ssu(val, SER_NETWORK, PROTOCOL_VERSION);
                try { ssu >> psgt.inputs[i].non_witness_utxo; }
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
        while (!ss.empty())
        {
            std::vector<unsigned char> key;
            ReadPSGTField(ss, key);
            if (key.empty()) break; // Separator

            std::vector<unsigned char> val;
            ReadPSGTField(ss, val);

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

    } catch (const std::exception& e) {
        // Any truncated/over-long length field or malformed embedded object
        // surfaces here as a stream exception rather than a crash.
        error = std::string("Malformed PSGT: ") + e.what();
        return false;
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
// A standard low-S DER-encoded ECDSA signature plus sighash byte is at most
// 72 bytes, 73 with its push opcode (the consensus maximum is one byte more,
// but SCRIPT_VERIFY_LOW_S is part of STANDARD_SCRIPT_VERIFY_FLAGS, so a
// high-S signature would not relay and is the wrong bound for a broadcast
// size estimate); a compressed pubkey is 33 bytes, 34 pushed.
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
        if (*result.fee < 0 && result.error.empty()) {
            // Don't overwrite a per-input error recorded above.
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

/**
 * Resolve the script the partial signatures of input `index` are made over
 * (the P2SH-unwrapped redeem script), requiring the carried previous
 * transaction to actually match prevout and, for P2SH, the redeem script to
 * hash to the script hash the funded output commits to. Returns false when
 * the input's signing context cannot be established trustworthily.
 */
static bool GetPSGTInputSignScript(const PartiallySignedTransaction& psgt,
                                   unsigned int index, CScript& sign_script)
{
    if (index >= psgt.inputs.size() || index >= psgt.tx.vin.size())
        return false;

    const PSGTInput& input = psgt.inputs[index];
    const COutPoint& prevout = psgt.tx.vin[index].prevout;

    if (input.non_witness_utxo.IsNull()
        || prevout.n >= input.non_witness_utxo.vout.size()
        || input.non_witness_utxo.GetHash() != prevout.hash)
        return false;

    const CScript& scriptPubKey = input.non_witness_utxo.vout[prevout.n].scriptPubKey;
    if (!scriptPubKey.IsPayToScriptHash())
    {
        sign_script = scriptPubKey;
        return true;
    }

    if (input.redeem_script.empty())
        return false;

    const CScriptID expected(uint160(std::vector<unsigned char>(
        scriptPubKey.begin() + 2, scriptPubKey.begin() + 22)));
    if (CScriptID(input.redeem_script) != expected)
        return false;

    sign_script = input.redeem_script;
    return true;
}

std::optional<CScriptID> GetPSGTInputImage(const PartiallySignedTransaction& psgt,
                                           unsigned int index)
{
    if (index >= psgt.inputs.size() || index >= psgt.tx.vin.size())
        return std::nullopt;

    const PSGTInput& input = psgt.inputs[index];
    const COutPoint& prevout = psgt.tx.vin[index].prevout;

    if (input.non_witness_utxo.IsNull()
        || prevout.n >= input.non_witness_utxo.vout.size()
        || input.non_witness_utxo.GetHash() != prevout.hash)
        return std::nullopt;

    // Only P2SH-wrapped multisig arrangements have an image.
    if (!input.non_witness_utxo.vout[prevout.n].scriptPubKey.IsPayToScriptHash())
        return std::nullopt;

    CScript sign_script;
    if (!GetPSGTInputSignScript(psgt, index, sign_script))
        return std::nullopt;

    txnouttype script_type;
    std::vector<std::vector<unsigned char>> vSolutions;
    if (!Solver(sign_script, script_type, vSolutions) || script_type != TX_MULTISIG)
        return std::nullopt;

    return CScriptID(sign_script);
}

std::optional<CScriptID> GetPSGTImage(const PartiallySignedTransaction& psgt)
{
    if (psgt.inputs.empty() || psgt.inputs.size() != psgt.tx.vin.size())
        return std::nullopt;

    std::optional<CScriptID> image = GetPSGTInputImage(psgt, 0);
    if (!image)
        return std::nullopt;

    for (unsigned int i = 1; i < psgt.inputs.size(); ++i)
    {
        const std::optional<CScriptID> other = GetPSGTInputImage(psgt, i);
        if (!other || *other != *image)
            return std::nullopt;
    }

    return image;
}

bool GetPSGTMultisigParams(const PartiallySignedTransaction& psgt,
                           int& required, int& total)
{
    if (!GetPSGTImage(psgt))
        return false;

    // GetPSGTImage established that input 0's redeem script is present,
    // committed to by the funded output, and TX_MULTISIG.
    txnouttype script_type;
    std::vector<std::vector<unsigned char>> vSolutions;
    if (!Solver(psgt.inputs[0].redeem_script, script_type, vSolutions)
        || script_type != TX_MULTISIG)
        return false;

    required = vSolutions.front()[0];
    total = vSolutions.size() - 2;
    return true;
}

bool VerifyPSGTPartialSigs(const PartiallySignedTransaction& psgt,
                           std::vector<std::set<CKeyID>>& valid_keys_per_input,
                           std::string& error)
{
    valid_keys_per_input.assign(psgt.inputs.size(), {});

    if (psgt.inputs.size() != psgt.tx.vin.size())
    {
        error = "PSGT input count does not match unsigned transaction input count";
        return false;
    }

    const CTransaction tx(psgt.tx);

    for (unsigned int i = 0; i < psgt.inputs.size(); ++i)
    {
        const PSGTInput& input = psgt.inputs[i];
        if (input.partial_sigs.empty())
            continue;

        CScript sign_script;
        if (!GetPSGTInputSignScript(psgt, i, sign_script))
        {
            error = strprintf("input %u: carries partial signatures but its signing "
                              "context cannot be verified (missing or mismatched "
                              "previous transaction or redeem script)", i);
            return false;
        }

        txnouttype script_type;
        std::vector<std::vector<unsigned char>> vSolutions;
        if (!Solver(sign_script, script_type, vSolutions) || script_type != TX_MULTISIG)
        {
            // Phase I only accumulates partial_sigs for multisig scripts;
            // anything else carrying them is malformed or crafted.
            error = strprintf("input %u: partial signatures on a non-multisig script", i);
            return false;
        }

        // Map the redeem script's pubkeys by key id so each partial_sigs
        // entry (keyed by CKeyID only) can be verified.
        std::map<CKeyID, std::vector<unsigned char>> member_pubkeys;
        for (unsigned int j = 1; j + 1 < vSolutions.size(); ++j)
        {
            const CPubKey pubkey(vSolutions[j]);
            if (pubkey.IsValid())
                member_pubkeys[pubkey.GetID()] = vSolutions[j];
        }

        for (const auto& [keyid, sig] : input.partial_sigs)
        {
            const auto member = member_pubkeys.find(keyid);
            if (member == member_pubkeys.end())
            {
                error = strprintf("input %u: partial signature by a key that is not "
                                  "part of the multisig arrangement", i);
                return false;
            }

            // Enforce the signature encoding the finalized transaction will have to
            // satisfy (strict DER + low-S + STRICTENC), not just ECDSA validity:
            // CheckSig verifies through the lax parser + S-normalization, so a
            // malleated (high-S / non-canonical-DER) copy would verify here yet be
            // rejected by the network's mandatory DERSIG on broadcast. Reject it now
            // so the pool never vouches for an un-finalizable PSGT.
            if (sig.empty()
                || !CheckSignatureEncoding(sig, STANDARD_SCRIPT_VERIFY_FLAGS)
                || !CheckSig(sig, member->second, sign_script, tx, i))
            {
                error = strprintf("input %u: invalid partial signature", i);
                return false;
            }

            valid_keys_per_input[i].insert(keyid);
        }
    }

    return true;
}

bool PSGTSignedBy(const SigningProvider& provider,
                  const PartiallySignedTransaction& psgt)
{
    const CTransaction tx(psgt.tx);

    for (unsigned int i = 0; i < psgt.inputs.size() && i < psgt.tx.vin.size(); ++i)
    {
        const PSGTInput& input = psgt.inputs[i];
        if (input.partial_sigs.empty())
            continue;

        // Tolerant per-input skip (not a hard failure like
        // VerifyPSGTPartialSigs): only provider-owned entries decide.
        CScript sign_script;
        if (!GetPSGTInputSignScript(psgt, i, sign_script))
            continue;

        txnouttype script_type;
        std::vector<std::vector<unsigned char>> vSolutions;
        if (!Solver(sign_script, script_type, vSolutions) || script_type != TX_MULTISIG)
            continue;

        for (unsigned int j = 1; j + 1 < vSolutions.size(); ++j)
        {
            const CPubKey pubkey(vSolutions[j]);
            if (!pubkey.IsValid() || !provider.HaveKey(pubkey.GetID()))
                continue;

            const auto it = input.partial_sigs.find(pubkey.GetID());
            if (it == input.partial_sigs.end() || it->second.empty())
                continue;

            // Same encoding-strictness gate as VerifyPSGTPartialSigs: a malleated
            // copy of an own signature would verify but not finalize, so it must
            // not be reported as the provider having signed.
            if (CheckSignatureEncoding(it->second, STANDARD_SCRIPT_VERIFY_FLAGS)
                && CheckSig(it->second, vSolutions[j], sign_script, tx, i))
                return true;
        }
    }

    return false;
}
