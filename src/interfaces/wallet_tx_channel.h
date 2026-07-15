// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_WALLET_TX_CHANNEL_H
#define GRIDCOIN_INTERFACES_WALLET_TX_CHANNEL_H

#include "interfaces/wallet_tx_filter.h"
#include "interfaces/wallet_tx_record.h"
#include "uint256.h"
#include "wallet/generated_type.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

class CWallet;

//! Boundary types for the wallet transaction channel (the windowed
//! transaction-table stack adopted as the multiprocess wallet-side transport
//! seam — doc/multiprocess_design.md §6). The event payloads, filter spec and
//! read-result types keep their historical GRC namespace; they are boundary
//! value types: pointer-free, Qt-free, marshalable. The abstract
//! interfaces::WalletTxSource (interfaces/wallet_tx_source.h) owns the store
//! behind these types; the GUI drives it through that handle and only ever
//! sees the value types declared here.

namespace GRC {

//!
//! \brief View identifiers stamped on per-view events (PR3).
//!
//! VIEW_FULL is the native unfiltered/unsorted stream the TransactionTableModel
//! consumes today (PR2/PR2.5 behaviour, epoch 0). Registered cursors (server-side
//! filter/sort) use VIEW_OVERVIEW and up. The consumer ignores events whose viewId
//! it does not own.
//!
constexpr int VIEW_FULL = 0;
constexpr int VIEW_OVERVIEW = 1;
constexpr int VIEW_DETAILED = 2;   //!< the detailed history view (PR4)

//!
//! \brief Producer-side payload: rows were inserted into the ordered wallet
//! view at a producer-computed position.
//!
//! Windowed-model PR2 moved the ORDERING to the producer-side GRC::WalletTxStore:
//! the producer runs TransactionRecord::decomposeTransaction under the locks it
//! already holds, the store computes the TxOrderLess insert position, and this
//! payload carries that position plus the records. The consumer inserts the
//! records into its replica at exactly \p position inside a single
//! beginInsertRows/endInsertRows bracket — it reads ONLY this payload, never the
//! live store, so its replica tracks the begin/end replay even when a drain
//! batch contains several inserts that reorder rows.
//!
//! \p records are all the decomposed parts of one transaction; under TxOrderLess
//! they occupy a contiguous range, so one payload == one bracket.
//!
struct RowsInsertedPayload
{
    int position;
    std::vector<TransactionRecord> records;
    int viewId = VIEW_FULL;   //!< which view this insert belongs to (PR3)
    uint64_t epoch = 0;       //!< source cursor epoch, for stale-fetch reconciliation (PR3)
};

//!
//! \brief Producer-side payload: a contiguous run of rows was removed at a
//! producer-computed position. Carries position + count (resolved from the tx
//! hash by the store, where same-hash rows are guaranteed contiguous). The
//! consumer erases [position, position + count) inside one
//! beginRemoveRows/endRemoveRows bracket. A removal that matched nothing emits
//! no event at all, so this payload always denotes a real, non-empty erase.
//!
struct RowsRemovedPayload
{
    int position;
    int count;
    int viewId = VIEW_FULL;   //!< which view this removal belongs to (PR3)
    uint64_t epoch = 0;       //!< source cursor epoch, for stale-fetch reconciliation (PR3)
};

//!
//! \brief Producer-side payload (PR3): a per-view cursor was rebuilt wholesale
//! (filter or sort change) — the consumer must drop its replica for \p viewId
//! and bulk-refill \p total served rows via the source's getRows. Carries the
//! new \p epoch so any in-flight pre-rebuild fetch can be discarded on arrival.
//!
struct RowsResetPayload
{
    int viewId;
    uint64_t epoch;
    int total;       //!< served-window row count after the rebuild
};

//!
//! \brief Producer-side payload (PR3): the TOTAL accepted row count for a view
//! changed (e.g. a row entered/left the filter off-window, so the scrollbar
//! extent moves but no served row was inserted/removed). The consumer resizes
//! its virtual rowCount to \p total_accepted without touching cached rows.
//! Decision 4 (RowCountChanged not deferred — OverviewPage is the windowed testbed).
//!
struct RowCountChangedPayload
{
    int viewId;
    uint64_t epoch;
    int total_accepted;
};

//!
//! \brief Producer-side payload (PR3): rows [first, first+count) of \p viewId
//! changed in place (a status/field update that did NOT move them) — the
//! consumer re-reads them (dataChanged), re-fetching from the source if windowed.
//!
struct RowsChangedPayload
{
    int viewId;
    uint64_t epoch;
    int first;
    int count;
};

//!
//! \brief Producer-side payload: the chain tip advanced (block connected or
//! disconnected). Replaces the role of the legacy 4-second
//! WalletModel::pollBalanceChanged poll which used to watch
//! nBestHeight != cachedNumBlocks. The consumer reacts by refreshing
//! per-row confirmation status and re-running the (rate-limited) balance
//! recompute path.
//!
//! Pre-computing the balance on the producer side is intentionally NOT done
//! here: wallet->GetBalance() iterates the full wallet (O(N) over mapWallet)
//! and we don't want to pay that on msghand for every block. The
//! consumer-side path keeps the existing TRY_LOCK + MODEL_UPDATE_DELAY-gated
//! recompute, but is now event-driven instead of timer-polled. When this
//! code becomes the consumer side of an IPC channel post multiprocess
//! separation, the producer can optionally precompute the snapshot to save
//! a round-trip; the consumer contract doesn't change.
//!
struct ChainTipChangedPayload
{
    int     height;
    int64_t best_time;
};

using WalletEventPayload = std::variant<
    RowsInsertedPayload,
    RowsRemovedPayload,
    ChainTipChangedPayload,
    RowsResetPayload,
    RowCountChangedPayload,
    RowsChangedPayload>;

//!
//! \brief A single event in the wallet→GUI event channel.
//!
//! The seqno is assigned by the source's event queue under its mutex, giving
//! every event a unique monotonic ordering across all producer threads.
//! Consumers rely on this for "resync from seqno N+1" semantics that will be
//! used once the channel becomes the consumer side of an IPC connection.
//!
//! emit_time_us is for diagnostics only — it lets the consumer measure
//! end-to-end queue latency without taking any extra locks.
//!
struct WalletEvent
{
    uint64_t           seqno;
    int64_t            emit_time_us;
    WalletEventPayload payload;
};

//!
//! \brief Atomic result of a windowed read (getRows): the requested row slice
//! plus the view's metadata, all sampled under the SAME store-lock hold.
//!
//! Returning the four together is the generalization of PR4-fix B to the PR5
//! scroll path. A windowed consumer keeps two reconciliation channels: a
//! STRUCTURAL channel (the virtual rowCount + the ordered Insert/Remove/Change/
//! Reset delta stream) and a CONTENT channel (the scroll-driven slice fetch).
//! The content fetch must observe the rowCount (\ref total_accepted), the
//! cursor's sort/filter generation (\ref epoch) and the view's event high-water
//! (\ref high_water) at the EXACT instant the rows were copied — a second,
//! separately-locked call could observe a different store state and misalign
//! the slice against the structural channel, dropping or double-counting a row
//! (the PR4-fix B bug class). The consumer adopts a content fetch only when its
//! \ref epoch and \ref high_water match the structural channel's, and NEVER
//! advances the structural seqno from it.
//!
struct RowsResult {
    std::vector<TransactionRecord> records; //!< the [first, first+count) slice (a copy)
    int total_accepted = 0;                 //!< the view's full accepted count (virtual rowCount)
    uint64_t epoch = 0;                      //!< cursor sort/filter generation at the read instant
    uint64_t high_water = 0;                 //!< seqno of the last event emitted for the view at the read
};

//!
//! \brief Structured transaction detail for the double-click dialog: every
//! fact the old node-side TransactionDesc::toHTML rendered, as typed value
//! fields. The node fills it under the wallet locks; the GUI renders (and
//! translates) it — no tr(), no HTML, no localization crosses the boundary
//! (multiprocess design §4.1). getRowDetail's node-side HTML rendering was
//! the one standing violation and is replaced by this DTO.
//!
struct WalletTxDetail
{
    //! False when the (hash, idx) key or the wallet transaction is gone; every
    //! other field is then meaningless and the dialog shows its fallback text.
    bool found = false;

    //! Finality classification, mirroring the old FormatTxStatus branches.
    enum class Finality {
        OpenForBlocks,   //!< not final; finality_value = blocks remaining
        OpenUntilTime,   //!< not final; finality_value = locktime timestamp
        Conflicted,      //!< depth < 0
        Offline,         //!< no broadcast success and stale receive time
        Unconfirmed,     //!< 0 <= depth < RecommendedNumConfirmations
        Confirmed,       //!< depth >= RecommendedNumConfirmations
    };
    Finality finality = Finality::Unconfirmed;
    int64_t finality_value = 0; //!< blocks remaining / locktime, per Finality
    int depth = 0;              //!< GetDepthInMainChain at fill time

    //! GetRequestCount(): -1 unknown, 0 not yet broadcast, >0 node count.
    int requests = -1;

    int64_t time = 0;           //!< GetTxTime(); 0 renders as empty

    //! Source/From classification, mirroring the old "From" section.
    enum class Source {
        Other,           //!< none of the below (no From line, or own-credit line)
        CoinBase,
        CoinStake,       //!< generated_type identifies the stake subtype
        OnlineFrom,      //!< mapValue["from"] present; from_value carries it
    };
    Source source = Source::Other;
    MinedType generated_type = MinedType::UNKNOWN;
    std::string from_value;

    //! The own-address credit line (net > 0 and the credited address is ours
    //! with an address-book entry): "From: unknown / To: <addr> (own address)".
    bool has_own_credit_line = false;
    std::string own_credit_address;
    std::string own_credit_label;

    //! mapValue["to"] (online transaction "To" line) + its address-book label.
    bool has_to = false;
    std::string to_value;
    std::string to_label;

    //! Amount-section classification, mirroring the old branch structure.
    enum class AmountForm {
        CoinbaseImmature,  //!< coinbase with zero credit: unmatured sum
        NetCredit,         //!< net > 0: single credit line of `net`
        FromMe,            //!< all inputs ours: per-output debits (+self +fee)
        Mixed,             //!< neither: per-input debits and per-output credits
    };
    AmountForm amount_form = AmountForm::NetCredit;

    // CoinbaseImmature:
    int64_t unmatured = 0;
    bool in_main_chain = false;
    int matures_in = 0;

    // FromMe: one entry per non-own output.
    struct DebitOut {
        bool has_address = false; //!< an extractable destination (offline tx "To" line)
        std::string address;
        std::string label;        //!< address-book label (may be empty)
        int64_t amount = 0;       //!< the output value (rendered negated)
    };
    std::vector<DebitOut> debits;
    bool to_self = false;         //!< all outputs also ours (payment to self)
    int64_t self_value = 0;       //!< credit - change, for the paired debit/credit lines
    bool has_fee = false;
    int64_t fee = 0;

    // Mixed: raw per-input debit and per-output credit amounts (ours only).
    std::vector<int64_t> mixed_debits;
    std::vector<int64_t> mixed_credits;

    int64_t net = 0;              //!< credit - debit (the Net amount line)

    std::string map_message;      //!< mapValue["message"]
    std::string map_comment;      //!< mapValue["comment"]
    std::string txid;
    bool has_block_hash = false;
    std::string block_hash;
    std::string contract_message; //!< GRC::GetMessage(wtx) — the tx-message contract

    //! Coinbase/coinstake extras: the stake data table (label/value pairs are
    //! untranslated data strings today) and the maturity notice paragraph.
    bool is_generated = false;
    std::vector<std::pair<std::string, std::string>> stake_info;

    //! The always-rendered debits/credits + raw-data section.
    std::vector<int64_t> verbose_debits;
    std::vector<int64_t> verbose_credits;
    std::string tx_dump;          //!< wtx.ToString()
    struct InputInfo {
        bool has_address = false;
        std::string address;
        int64_t amount = 0;
        bool is_mine = false;
    };
    std::vector<InputInfo> inputs;
};

} // namespace GRC
#endif // GRIDCOIN_INTERFACES_WALLET_TX_CHANNEL_H
