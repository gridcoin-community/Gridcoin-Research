// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_PSGT_H
#define GRIDCOIN_PSGT_H

#include <primitives/transaction.h>
#include <script/sign.h>
#include <pubkey.h>

#include <map>
#include <optional>
#include <set>
#include <string>
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

// Upper bound on the serialized (binary) size of a PSGT accepted from
// untrusted sources (network relay, pool submission). Generous for a
// multisig spend of a few dozen inputs, each carrying its full previous
// transaction, while bounding per-object memory.
static constexpr size_t MAX_PSGT_WIRE_SIZE = 100000;

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
 * Decode a PSGT from base64 text (whitespace tolerated).
 * @return true on success.
 */
bool DecodeRawPSGT(PartiallySignedTransaction& psgt,
                    const std::string& base64_tx,
                    std::string& error);

/**
 * Decode a PSGT from raw binary bytes (the SerializePSGT format). This is
 * the transport-agnostic core of DecodeRawPSGT, exposed for callers that
 * receive PSGT bytes directly (e.g. network relay) rather than base64 text.
 * @return true on success.
 */
bool DecodePSGTBytes(PartiallySignedTransaction& psgt,
                     const std::vector<unsigned char>& data,
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

/** Roles in the PSGT workflow, in pipeline order. */
enum class PSGTRole {
    CREATOR,
    UPDATER,
    SIGNER,
    FINALIZER,
    EXTRACTOR,
};

/** Lowercase display name of a PSGT role ("creator", "updater", ...). */
std::string PSGTRoleName(PSGTRole role);

/** Analysis result for a single PSGT input. */
struct PSGTInputAnalysis
{
    bool has_utxo = false;  //!< non_witness_utxo present, prevout.n in range, hash matches
    bool is_final = false;  //!< final_script_sig present
    PSGTRole next = PSGTRole::UPDATER; //!< Next role needed to make progress on this input

    std::vector<CKeyID> missing_pubkeys; //!< Keys whose full pubkey is not in the PSGT
    std::vector<CKeyID> missing_sigs;    //!< Keys whose signature is still required
    bool missing_redeem_script = false;  //!< P2SH input whose redeem script is unknown
};

/** Whole-PSGT analysis result (Bitcoin Core analyzepsbt equivalent). */
struct PSGTAnalysis
{
    std::optional<CAmount> fee;                       //!< Inputs minus outputs; set iff all input UTXOs are known
    std::optional<unsigned int> estimated_final_size; //!< Bytes of the fully-signed tx; set iff every input's final size can be estimated
    std::optional<CAmount> min_required_fee;          //!< GetMinFee at the estimated final size
    std::vector<PSGTInputAnalysis> inputs;
    PSGTRole next = PSGTRole::EXTRACTOR; //!< Earliest-stage role needed across all inputs
    std::string error;                   //!< Non-empty if a problem was detected
};

/**
 * Analyze a PSGT: per-input UTXO/finality status, missing material
 * (pubkeys, signatures, redeem scripts), the next role required per input
 * and globally, plus fee and estimated final size when computable.
 * Pure function of the PSGT; does not consult the wallet or mutate anything.
 */
PSGTAnalysis AnalyzePSGT(const PartiallySignedTransaction& psgtx);

/**
 * The multisig "image" of one PSGT input: the hash of its redeem script
 * (CScriptID), the key a PSGT pool indexes on (#2910). Returns a value only
 * when the input is a self-consistent P2SH multisig: the carried previous
 * transaction matches prevout, the funded output is P2SH, the redeem script
 * is present and hashes to the script hash the output commits to, and the
 * redeem script decomposes as m-of-n CHECKMULTISIG.
 *
 * IMPORTANT — this establishes INTERNAL CONSISTENCY only, not on-chain reality.
 * The non_witness_utxo is attacker-supplied; these checks prove the carried
 * previous transaction commits to this redeem script, NOT that it is confirmed
 * or a live UTXO. A caller that trusts the image to gate real resources (the
 * #2910/#3115 pool) MUST additionally verify each input's prevout against the
 * live UTXO set (exists AND unspent) before trusting it; otherwise an image can
 * be fabricated for a multisig that was never funded on-chain.
 */
std::optional<CScriptID> GetPSGTInputImage(const PartiallySignedTransaction& psgt,
                                           unsigned int index);

/**
 * The single multisig image shared by ALL inputs of the PSGT, or nullopt if
 * any input lacks an image or two inputs disagree. One image per PSGT is the
 * pool's identity model: multi-input spends from the same multisig address
 * qualify; mixed-arrangement PSGTs do not.
 */
std::optional<CScriptID> GetPSGTImage(const PartiallySignedTransaction& psgt);

/**
 * Recover the m-of-n parameters of the PSGT's (single) multisig image.
 * @return false if GetPSGTImage would return nullopt.
 */
bool GetPSGTMultisigParams(const PartiallySignedTransaction& psgt,
                           int& required, int& total);

/**
 * Cryptographically verify EVERY partial signature carried by the PSGT.
 * For each input, each (key id, signature) entry must map to a pubkey of the
 * input's multisig redeem script and pass CheckSig over the unsigned
 * transaction. Any unknown key or invalid signature fails the whole PSGT —
 * partial_sigs are untrusted after transport and CombinePSGTs (which merges
 * without verifying).
 *
 * On success, valid_keys_per_input[i] holds the key ids with a verified
 * signature on input i (the per-input m-of-n progress).
 */
bool VerifyPSGTPartialSigs(const PartiallySignedTransaction& psgt,
                           std::vector<std::set<CKeyID>>& valid_keys_per_input,
                           std::string& error);

/**
 * True iff some input carries a cryptographically valid partial signature
 * from a key the provider holds — verified against the input's sighash, not
 * merely present under an owned key id. Tolerant of other signers' invalid
 * or unverifiable material (unlike VerifyPSGTPartialSigs): only the
 * provider-owned entries decide the result. This is the ">=1 valid own
 * signature" precondition for submitting a PSGT to the pool (#2910).
 */
bool PSGTSignedBy(const SigningProvider& provider,
                  const PartiallySignedTransaction& psgt);

#endif // GRIDCOIN_PSGT_H
