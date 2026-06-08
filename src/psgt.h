// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_PSGT_H
#define GRIDCOIN_PSGT_H

#include <primitives/transaction.h>
#include <script/sign.h>
#include <pubkey.h>

#include <map>
#include <vector>

// BIP 174-style key types for PSGT serialization.
// Global map keys:
static constexpr uint8_t PSGT_GLOBAL_UNSIGNED_TX = 0x00;

// Per-input map keys:
static constexpr uint8_t PSGT_IN_NON_WITNESS_UTXO = 0x00;
static constexpr uint8_t PSGT_IN_PARTIAL_SIG       = 0x02;
static constexpr uint8_t PSGT_IN_SIGHASH_TYPE      = 0x03;
static constexpr uint8_t PSGT_IN_REDEEM_SCRIPT     = 0x04;
static constexpr uint8_t PSGT_IN_BIP32_DERIVATION  = 0x06;
static constexpr uint8_t PSGT_IN_FINAL_SCRIPTSIG   = 0x07;

// Per-output map keys:
static constexpr uint8_t PSGT_OUT_REDEEM_SCRIPT    = 0x00;
static constexpr uint8_t PSGT_OUT_BIP32_DERIVATION = 0x02;

// Magic bytes: "psgt" + 0xff separator
static const std::vector<unsigned char> PSGT_MAGIC = {0x70, 0x73, 0x67, 0x74, 0xff};

/** HD key origin information: fingerprint + derivation path. */
struct KeyOriginInfo
{
    unsigned char fingerprint[4] = {0};
    std::vector<uint32_t> path;
};

/** A PSGT input: metadata about one transaction input. */
struct PSGTInput
{
    CTransaction non_witness_utxo;                           // Full previous transaction
    CScript redeem_script;                                    // For P2SH inputs
    std::map<CPubKey, KeyOriginInfo> hd_keypaths;            // HD derivation info
    std::map<CKeyID, std::vector<unsigned char>> partial_sigs; // Partial signatures
    int sighash_type = 0;                                     // Sighash type (0 = not set)
    CScript final_script_sig;                                 // Finalized scriptSig
    std::map<std::vector<unsigned char>,
             std::vector<unsigned char>> unknown;             // Unknown key-value pairs
};

/** A PSGT output: metadata about one transaction output. */
struct PSGTOutput
{
    CScript redeem_script;
    std::map<CPubKey, KeyOriginInfo> hd_keypaths;
    std::map<std::vector<unsigned char>,
             std::vector<unsigned char>> unknown;
};

/** A Partially Signed Gridcoin Transaction. */
struct PartiallySignedTransaction
{
    CMutableTransaction tx;
    std::vector<PSGTInput> inputs;
    std::vector<PSGTOutput> outputs;
    std::map<std::vector<unsigned char>,
             std::vector<unsigned char>> unknown;

    PartiallySignedTransaction() {}
    explicit PartiallySignedTransaction(const CMutableTransaction& txIn);
};

/** Check whether an input is fully signed (has final_script_sig). */
bool PSGTInputSigned(const PSGTInput& input);

/**
 * Sign a single PSGT input using the given signing provider.
 * @return true if the input was successfully signed (or was already signed).
 */
bool SignPSGTInput(const SigningProvider& provider,
                   PartiallySignedTransaction& psgt,
                   unsigned int index,
                   int sighash = SIGHASH_ALL);

/**
 * Finalize a PSGT: for each input, if partial signatures can produce a
 * complete scriptSig, assemble it and store in final_script_sig.
 * @return true if all inputs were finalized.
 */
bool FinalizePSGT(PartiallySignedTransaction& psgt);

/**
 * Finalize and extract: finalize then produce the completed raw transaction.
 * @return true if successful (all inputs finalized and tx extracted).
 */
bool FinalizeAndExtractPSGT(PartiallySignedTransaction& psgt, CMutableTransaction& result);

/**
 * Combine multiple PSGTs for the same unsigned transaction into one.
 * @return true if all PSGTs are compatible and were merged successfully.
 */
bool CombinePSGTs(PartiallySignedTransaction& out,
                   const std::vector<PartiallySignedTransaction>& psgts);

/**
 * Decode a PSGT from a binary stream.
 * @return true on success.
 */
bool DecodeRawPSGT(PartiallySignedTransaction& psgt,
                    const std::string& base64_tx,
                    std::string& error);

/**
 * Serialize a PSGT to a binary byte vector.
 */
std::vector<unsigned char> SerializePSGT(const PartiallySignedTransaction& psgt);

/**
 * Update a PSGT output with redeem script and HD keypath info from the
 * signing provider (typically the wallet). For P2SH outputs, looks up the
 * redeem script. For all outputs, looks up HD keypaths for involved pubkeys.
 */
void UpdatePSGTOutput(const SigningProvider& provider,
                       PartiallySignedTransaction& psgt,
                       unsigned int index);

#endif // GRIDCOIN_PSGT_H
