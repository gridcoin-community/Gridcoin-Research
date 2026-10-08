#ifndef GRIDCOIN_INTERFACES_WALLET_TX_RECORD_H
#define GRIDCOIN_INTERFACES_WALLET_TX_RECORD_H

#include "interfaces/marshal.h"
#include "uint256.h"
#include "wallet/generated_type.h"
#include "wallet/ismine.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

class CWallet;
class CWalletTx;

/** UI model for transaction status. The transaction status is the part of a transaction that will change over time.
 */
class TransactionStatus
{
public:
    TransactionStatus()
        : countsForBalance(false)
        , sortKey("")
        , matures_in(0)
        , status(Offline)
        , generated_type(GRC::MinedType::UNKNOWN)
        , depth(0)
        , open_for(0)
        , cur_num_blocks(-1)
    { }

    enum Status {
        Confirmed,          /**< Have 10 or more confirmations (normal tx) or fully mature (mined tx) **/
        /// Normal (sent/received) transactions
        OpenUntilDate,      /**< Transaction not yet final, waiting for date */
        OpenUntilBlock,     /**< Transaction not yet final, waiting for block */
        Offline,            /**< Not sent to any other nodes **/
        Unconfirmed,        /**< Not yet mined into a block **/
        Confirming,         /**< Confirmed, but waiting for the recommended number of confirmations **/
        Conflicted,         /**< Conflicts with other transaction or mempool **/
        /// Generated (mined) transactions
        Immature,           /**< Mined but waiting for maturity */
        MaturesWarning,     /**< Transaction will likely not mature because no nodes have confirmed */
        NotAccepted         /**< Mined but not accepted */
    };

    /// Transaction counts towards available balance
    bool countsForBalance;
    /// Sorting key based on status
    std::string sortKey;

    /** @name Generated (mined) transactions
       @{*/
    int matures_in;
    /**@}*/

    /** @name Reported status
       @{*/
    Status status;
    GRC::MinedType generated_type;
    int64_t depth;
    int64_t open_for; /**< Timestamp if status==OpenUntilDate, otherwise number
                       of additional blocks that need to be mined before
                       finalization */
    /**@}*/

    /** Current number of blocks (to know whether cached status is still valid) */
    int cur_num_blocks;
};

/** UI model for a transaction. A core transaction can be represented by multiple UI transactions if it has
    multiple outputs.
 */
class TransactionRecord
{
public:
    enum Type
    {
        Other,
        Generated,
        SendToAddress,
        SendToOther,
        RecvWithAddress,
        RecvFromOther,
        SendToSelf,
        BeaconAdvertisement,
        Poll,
        Vote,
        Message,
        MRC
    };

    static constexpr std::initializer_list<Type> TYPES {Other,
                                                       Generated,
                                                       SendToAddress,
                                                       RecvWithAddress,
                                                       RecvFromOther,
                                                       SendToSelf,
                                                       BeaconAdvertisement,
                                                       Poll,
                                                       Vote,
                                                       Message,
                                                       MRC};

    /** Number of confirmation recommended for accepting a transaction */
    static const int RecommendedNumConfirmations = 10;

    TransactionRecord():
            hash(), time(0), type(Other), address(""), debit(0), credit(0), vout(0), idx(0)
    {
    }

    TransactionRecord(uint256 hash, int64_t time):
            hash(hash), time(time), type(Other), address(""), debit(0),
            credit(0), vout(0), idx(0)
    {
    }

    TransactionRecord(uint256 hash, int64_t time,
                Type type, const std::string &address,
                int64_t debit, int64_t credit, unsigned int vout):
            hash(hash), time(time), type(type), address(address), debit(debit), credit(credit),
            vout(vout), idx(0)
    {
    }

    /** Decompose CWallet transaction to model transaction records.
     */
    static bool showTransaction(const CWalletTx &wtx, bool datetime_limit_flag = false, const int64_t &datetime_limit = 0);
    static std::vector<TransactionRecord> decomposeTransaction(const CWallet *wallet, const CWalletTx &wtx);

    /** @name Immutable transaction attributes
      @{*/
    uint256 hash;
    int64_t time;
    Type type;
    std::string address;
    int64_t debit;
    int64_t credit;

    // Side/Split Stake
    unsigned int vout;
    /**@}*/

    /** Subtransaction index, for sort key */
    int idx;

    /** Status: can change with block chain update */
    TransactionStatus status;

    /** Return the unique identifier for this transaction (part) */
    std::string getTxID() const;

    /** Update status from core wallet tx.
     */
    void updateStatus(const CWalletTx &wtx);

    /** Populate `label` from the wallet address book (Qt-free; caller must hold
     *  cs_wallet). Called producer-side at the fill points after updateStatus()
     *  so the off-cs_wallet cursor can sort the Address column and filter by
     *  label without re-touching the wallet (windowed-model PR4).
     */
    void populateDisplayLabel(const CWallet& wallet);

    /** Return whether a status update is needed.
     */
    bool statusUpdateNeeded();

    /** Cached address-book label for `address`, snapshotted producer-side at fill
     *  time (windowed-model PR4). Empty if `address` has no address-book entry.
     *  Used for the Address sort key and the address/label substring filter.
     */
    std::string label;
};

//! Marshalability pins (multiprocess design §4.1, windowed-model design): these
//! records are the wallet-transaction-channel DTOs and must stay value types a
//! node-side source can fill and ship across the interfaces:: boundary — no Qt
//! types, no pointers into core state, freely copyable. The translated type
//! rendering lives GUI-side (transactionview/transactiontablemodel), not here.
//! The fixed-width pin additionally locks the amount/time wire representation.
INTERFACES_ASSERT_MARSHALABLE(TransactionRecord);
INTERFACES_ASSERT_MARSHALABLE(TransactionStatus);
static_assert(std::is_same_v<decltype(TransactionRecord::time), int64_t>
                  && std::is_same_v<decltype(TransactionRecord::debit), int64_t>
                  && std::is_same_v<decltype(TransactionRecord::credit), int64_t>,
              "TransactionRecord amounts/time are fixed-width value types");

namespace GRC {
//!
//! \brief The status a snapshot implies at tip height \p height, assuming nothing but
//! height has changed since it was taken.
//!
//! The producer (WalletTxStore::applyChainTipRefresh) and the GUI's formatters share this
//! one projection. The producer sends a status Change only when a fresh status differs from
//! the projection of the previous one; the GUI projects the snapshot it holds to the pushed
//! tip height. Between status flips the two agree by construction (#3059).
//!
//! Per block, with no flip, TransactionRecord::updateStatus moves cur_num_blocks, depth (+1
//! while in the main chain) and matures_in (-1 while Immature or MaturesWarning; depth +
//! matures_in is constant). This projects all three except matures_in for MaturesWarning:
//! that status renders neither field, and leaving it unprojected keeps such a row ungated
//! (it is re-sent every block) -- conservative, and unreachable for coinstakes today, since
//! GetRequestCount() is -1 for any block not committed by this wallet. Everything else -- the
//! status enum, countsForBalance, sortKey (the containing block's height), generated_type --
//! changes only through an event. open_for is deliberately not projected: a non-final
//! transaction is non-standard, never in the mempool, and so never refreshed; projecting it
//! would count down to a false value.
//!
//! Identity when the snapshot has no height (cur_num_blocks < 0) or \p height is below it.
//! The second case covers a tip that moved back (a reorg; only a fresh updateStatus is valid)
//! AND a snapshot fetched by the GUI that is newer than the GUI's cached tip height (a scroll
//! fetch can land between drains). In both the snapshot as-is is the best answer.
//!
inline TransactionStatus ProjectTxStatus(const TransactionStatus& status, int height)
{
    TransactionStatus out = status;
    if (status.cur_num_blocks < 0 || height < status.cur_num_blocks) {
        return out;
    }
    const int delta = height - status.cur_num_blocks;
    out.cur_num_blocks = height;
    // depth <= 0 is not height-driven: 0 is the mempool, < 0 is conflicted or not in the
    // main chain.
    if (status.depth > 0) {
        out.depth += delta;
    }
    if (status.status == TransactionStatus::Immature) {
        out.matures_in -= delta;
    }
    return out;
}

//! Field-by-field equality over every member of TransactionStatus.
inline bool TxStatusEquals(const TransactionStatus& a, const TransactionStatus& b)
{
    return a.countsForBalance == b.countsForBalance
        && a.sortKey == b.sortKey
        && a.matures_in == b.matures_in
        && a.status == b.status
        && a.generated_type == b.generated_type
        && a.depth == b.depth
        && a.open_for == b.open_for
        && a.cur_num_blocks == b.cur_num_blocks;
}

//!
//! \brief The status to DISPLAY for a snapshot at the GUI's cached tip height.
//!
//! ProjectTxStatus with the height step bounded by the snapshot's category, so every displayed
//! field stays consistent with the status enum until the flip event arrives. A flip can trail
//! the tip height briefly: the GUI caches the newest tip of a drained batch before applying
//! the batch's events, a batch cap can split a tip from its Changes, and an intake upsert
//! computed at an older height can land after the refresh. Without the bound a Confirming row
//! would read "Confirming (10 of 10)" and an Immature row would run past maturity.
//!
//!  - Confirming: depth stops at RecommendedNumConfirmations - 1.
//!  - Immature:   at least one block left; depth + matures_in stays constant.
//!  - Confirmed:  unbounded (terminal).
//!  - anything else: the snapshot as-is.
//!
inline TransactionStatus DisplayTxStatus(const TransactionStatus& status, int height)
{
    if (status.cur_num_blocks < 0 || height < status.cur_num_blocks) {
        return status;
    }
    int step = height - status.cur_num_blocks;
    switch (status.status) {
    case TransactionStatus::Confirming: {
        const int64_t room = TransactionRecord::RecommendedNumConfirmations - 1 - status.depth;
        step = static_cast<int>(std::max<int64_t>(0, std::min<int64_t>(step, room)));
        break;
    }
    case TransactionStatus::Immature:
        step = std::max(0, std::min(step, status.matures_in - 1));
        break;
    case TransactionStatus::Confirmed:
        break;
    default:
        return status;
    }
    return ProjectTxStatus(status, status.cur_num_blocks + step);
}
} // namespace GRC

#endif // GRIDCOIN_INTERFACES_WALLET_TX_RECORD_H
