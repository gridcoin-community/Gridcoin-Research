// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_PSGT_H
#define GRIDCOIN_INTERFACES_PSGT_H

#include "amount.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CWallet;

namespace interfaces {

//! A serialized PSGT (the core SerializePSGT() byte format). The PSGT crosses the
//! interface boundary as these bytes rather than a live core
//! PartiallySignedTransaction, so the Qt layer holds no core PSGT type. The node
//! side reconstructs it with DecodePSGTBytes(). Base64 text I/O stays GUI-side.
using PSGTBytes = std::vector<unsigned char>;

//! Outcome of signing a pooled entry by image (mirror of core PSGTSignResult,
//! node/psgt_pool.h). The GUI maps each to translated text.
enum class PSGTSignStatus
{
    SIGNED_AND_RELAYED,      //!< Added signature(s); still incomplete; new revision pooled and relayed.
    COMPLETED_AND_BROADCAST, //!< Signature(s) completed the PSGT; final transaction broadcast; pool slot freed.
    NO_NEW_SIGNATURES,       //!< The wallet holds no key that could add a signature.
    ALREADY_KNOWN,           //!< Added signature(s), but the resulting revision is already pooled; nothing to relay.
    NOT_FOUND,               //!< The image is not in the pool.
    FAILED,                  //!< See error.
};

//! Outcome of adding a PSGT to the pool (mirror of core PSGTPoolAddResult).
enum class PSGTPoolAddStatus
{
    ACCEPTED_NEW,
    ACCEPTED_REPLACEMENT,
    DUPLICATE,
    REJECTED_POOL_FULL,
    REJECTED_NOT_BETTER,
    REJECTED_NOT_INITIATOR,
};

//! Why a candidate PSGT was rejected for the pool (mirror of core PSGTPoolReject).
//! Carried on a submit that never reached the add step so the GUI can explain it.
enum class PSGTPoolRejectReason
{
    NONE,
    TOO_LARGE,
    MALFORMED,
    HAS_UNKNOWN_FIELDS,
    DUPLICATE_REVISION,
    STRUCTURAL,
    INVALID_SIG,
    NO_VALID_SIG,
    COMPLETE,
    UTXO_MISSING,
    UTXO_SPENT,
    FEE_TOO_LOW,
    FEE_ABSURD,
};

//! Why a pooled entry was removed (mirror of core PSGTRemovalReason, same order/
//! values). The core PSGTPoolChanged signal delivers this as an int on removal; the
//! Qt model casts that int to this enum to label a history row without pulling in
//! node/psgt_pool.h.
enum class PSGTRemovalReason
{
    EXPIRED,
    REPLACED,
    COMPLETED,
    CONFLICT_MEMPOOL,
    CONFLICT_BLOCK,
    LOCAL_REMOVE,
};

//! Whether the PSGT pool is available on this node, with the reason when not —
//! replaces the GUI's IsV15Enabled/OutOfSyncByAge reads under cs_main. The page
//! banner distinguishes pre-v15 from out-of-sync; the buttons/submit gate on active.
struct PSGTPoolStatus
{
    bool active = false;       //!< v15 enabled AND not out of sync.
    bool v15_enabled = false;
    bool out_of_sync = false;
};

//! Wallet-relevance of a pool row, precomputed node-side (needs cs_wallet) so the
//! Qt model never touches pwalletMain. Drives the row's status presentation.
enum class PSGTRelevance
{
    NOT_MINE,              //!< No wallet key participates in this multisig.
    MINE_AWAITING_YOU,     //!< Wallet can add a signature and has not yet.
    MINE_AWAITING_OTHERS,  //!< Wallet has signed; waiting on other participants.
};

//! One row of the PSGT pool table (replaces g_psgt_pool.GetAll() + the former
//! in-model MakeRow derivation). All core-type reads (CScriptID/uint256, the
//! payment-destination vout walk, the wallet-relevance derivation) happen
//! node-side; the Qt model maps this to its display Row.
struct PSGTPoolRow
{
    std::string image_hex;     //!< The pool key / command id (CScriptID::ToString(), raw Hash160).
    std::string image_address; //!< The arrangement's P2SH address (EncodeDestination), for the Image column.
    std::string revision_hex;  //!< The revision hash (matched against the removal signal).
    std::string destination;   //!< Largest-non-change-output address, precomputed.
    CAmount amount = 0;
    int valid_sigs = 0;
    int sigs_required = 0;
    int sigs_total = 0;
    int64_t time_received = 0;
    bool in_pool = false;
    PSGTRelevance relevance = PSGTRelevance::NOT_MINE;
};

//! Signing state of one PSGT input, for the workbench description.
enum class PSGTInputSigState
{
    UNSIGNED,
    PARTIAL,    //!< partial_sig_count carries how many.
    FINALIZED,
};

//! State of an input's prev-tx amount, for the workbench description.
enum class PSGTInputAmountState
{
    NOT_LOADED,   //!< No non_witness_utxo — "(prev tx not loaded)".
    MISMATCH,     //!< non_witness_utxo present but does not match the prevout — "(prev tx mismatch)".
    HAVE,         //!< amount is valid.
};

//! One input of a described PSGT (flattened from PSGTInput + the tx vin; replaces
//! MultisignPSGTDialog::DescribePSGT's per-input core walk).
struct PSGTInputInfo
{
    std::string prevout_hash;
    uint32_t prevout_n = 0;
    bool has_metadata = false;  //!< The PSGT carries a PSGTInput for this vin ("no metadata" otherwise).
    PSGTInputAmountState amount_state = PSGTInputAmountState::NOT_LOADED;
    CAmount amount = 0;         //!< Valid when amount_state == HAVE.
    PSGTInputSigState sig_state = PSGTInputSigState::UNSIGNED;
    int partial_sig_count = 0;  //!< When sig_state == PARTIAL.
    bool has_redeem = false;    //!< The input carries a (non-empty) redeem_script.
    std::string p2sh_address;   //!< EncodeDestination(CScriptID(redeem_script)), when has_redeem.
    std::string p2sh_hash_hex;  //!< The raw Hash160 (the pool image), when has_redeem.
    bool is_multisig = false;   //!< redeem_script solves as TX_MULTISIG.
    int multisig_m = 0;         //!< Threshold, when is_multisig.
    int multisig_n = 0;         //!< Participant count, when is_multisig.
};

//! One output of a described PSGT.
struct PSGTOutputInfo
{
    bool is_standard = false;   //!< ExtractDestinations succeeded ("(non-standard)" otherwise).
    std::vector<std::string> destinations;
    int n_required = 0;         //!< >1 renders "N-of-M".
    CAmount amount = 0;
};

//! A flattened, display-ready description of a PSGT (replaces the dialog's
//! DescribePSGT walking a live core PSGT). Amounts are raw CAmount; the GUI
//! formats them with BitcoinUnits.
struct PSGTDescription
{
    bool valid = false;         //!< False if the bytes did not decode; error carries why.
    std::string error;
    std::string txid;
    int32_t version = 0;
    uint32_t time = 0;          //!< GRC tx nTime.
    uint32_t lock_time = 0;
    std::vector<PSGTInputInfo> inputs;
    std::vector<PSGTOutputInfo> outputs;
    int signed_input_count = 0;
    int input_count = 0;
    bool complete = false;      //!< FinalizePSGT(copy) succeeds.
};

//! Result of signing a working PSGT blob (the dialog's Sign action). Carries the
//! counts the status line reports; the GUI builds the translated message.
struct PSGTSignResult
{
    PSGTBytes psgt;             //!< The updated PSGT (echoed back to the workbench).
    bool ok = false;            //!< False only on a hard failure (wallet unavailable etc.); error set.
    bool complete = false;      //!< FinalizePSGT(copy) succeeds.
    int signed_now = 0;         //!< Inputs this wallet added a signature to this round.
    int needs_data = 0;         //!< Inputs still lacking a prev tx / redeem script.
    int could_not_sign = 0;     //!< Inputs the wallet holds a key for but could not sign.
    std::string error;
};

//! Result of combining PSGT blobs (the dialog's Combine action).
struct PSGTCombineResult
{
    PSGTBytes psgt;
    bool ok = false;
    std::string error;
};

//! Result of submitting a PSGT blob to the pool (the dialog's Submit action).
struct PSGTSubmitResult
{
    bool decoded = false;    //!< False if the bytes did not decode; error carries why.
    bool validated = true;   //!< False if ValidatePSGTForPool rejected before add; reject_reason/reject_text/error set.
    PSGTPoolAddStatus add_status = PSGTPoolAddStatus::DUPLICATE;
    PSGTPoolRejectReason reject_reason = PSGTPoolRejectReason::NONE;
    std::string reject_text; //!< PSGTPoolRejectToString(reject) (fixed label), when !validated.
    std::string revision_hex;
    std::string error;       //!< Core validation/add reason string, interpolated into a GUI tr() template.
    bool accepted = false;   //!< add_status is one of the ACCEPTED_* cases (relayed).
};

//! Result of finalizing a PSGT blob to a broadcastable raw transaction (Finalize).
struct PSGTFinalizeResult
{
    std::string raw_tx_hex;
    bool complete = false;
    std::string error;
};

//! Result of decoding imported PSGT bytes (validate a pasted PSGT before use).
struct PSGTDecodeResult
{
    PSGTBytes psgt;   //!< The canonical re-serialized bytes on success.
    bool ok = false;
    std::string error;
};

//! The PSGT pool + multisig-workbench boundary (Phase 1d-v). Covers the pool
//! table reads/commands (PSGTPoolPage / PSGTPoolTableModel) and the PSGT
//! workbench operations (MultisignPSGTDialog), so neither Qt component holds a
//! core PartiallySignedTransaction, touches g_psgt_pool / pwalletMain, or takes
//! cs_main / cs_wallet directly. Named ...Context to avoid the core PSGTPool
//! class collision. Over the node's single wallet (needed for sign / relevance).
class PSGTPoolContext
{
public:
    virtual ~PSGTPoolContext() = default;

    // --- Pool table (PSGTPoolPage / PSGTPoolTableModel) ---

    //! Snapshot of the pool as value rows (replaces g_psgt_pool.GetAll() and the
    //! former in-model wallet-relevance / payment-destination derivation).
    virtual std::vector<PSGTPoolRow> entries() = 0;

    //! Sign a pooled entry identified by its image hex (SignAndAdvancePSGT). The
    //! caller must hold the wallet unlocked. \p txid receives the broadcast
    //! transaction id (only meaningful for COMPLETED_AND_BROADCAST).
    virtual PSGTSignStatus signPoolEntry(const std::string& image_hex, std::string& error,
                                         std::string& txid) = 0;

    //! Locally remove a pooled entry by image hex (LOCAL_REMOVE).
    virtual bool removePoolEntry(const std::string& image_hex) = 0;

    //! Whether the pool is active for this node, with the reason when not
    //! (replaces the IsV15Enabled/OutOfSyncByAge reads under cs_main).
    virtual PSGTPoolStatus poolStatus() = 0;

    //! The serialized PSGT for a pooled entry by image hex, for the workbench
    //! Details action (replaces handing PSGTPoolEntry::psgt to the dialog).
    //! Returns empty bytes if the image is not pooled.
    virtual PSGTBytes poolEntryPsgt(const std::string& image_hex) = 0;

    // --- PSGT workbench (MultisignPSGTDialog) ---

    //! Flattened, display-ready description of a PSGT blob (DescribePSGT node-side).
    virtual PSGTDescription describePSGT(const PSGTBytes& psgt) = 0;

    //! Sign the working PSGT with this wallet (SignPSGTInput per input + analysis).
    //! The caller must hold the wallet unlocked.
    virtual PSGTSignResult signPSGT(const PSGTBytes& psgt) = 0;

    //! Combine several PSGTs into one (CombinePSGTs).
    virtual PSGTCombineResult combinePSGTs(const std::vector<PSGTBytes>& psgts) = 0;

    //! Validate and add the working PSGT to the pool, relaying on accept
    //! (ValidatePSGTForPool + g_psgt_pool.Add + RelayPSGT).
    virtual PSGTSubmitResult submitPSGTToPool(const PSGTBytes& psgt) = 0;

    //! Finalize the working PSGT and extract the broadcastable raw transaction hex.
    virtual PSGTFinalizeResult finalizeToRawTxHex(const PSGTBytes& psgt) = 0;

    //! Validate imported PSGT bytes (the dialog DecodeBase64's the pasted text
    //! first), returning canonical bytes or an error.
    virtual PSGTDecodeResult decodePSGT(const PSGTBytes& bytes) = 0;

    //! Whether this wallet already contributed at least one valid signature
    //! (PSGTSignedBy) — the pool-submit precondition.
    virtual bool walletHasSignature(const PSGTBytes& psgt) = 0;
};

//! Return an in-process PSGTPoolContext over the global PSGT pool and the node's
//! single wallet. \p wallet must outlive the object.
std::unique_ptr<PSGTPoolContext> MakePSGTPoolContext(CWallet* wallet);

} // namespace interfaces

#endif // GRIDCOIN_INTERFACES_PSGT_H
