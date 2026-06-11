// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_QT_TXFILTER_H
#define GRIDCOIN_QT_TXFILTER_H

#include <cstdint>
#include <string>

//!
//! \file txfilter.h
//!
//! Qt-free transaction-list filter/sort evaluation core.
//!
//! This is the first piece of the windowed transaction-table model (the
//! design doc's "Phase 3"). It extracts the predicate logic that today lives
//! inside TransactionFilterProxy::filterAcceptsRow() and the Qt::EditRole
//! sort keys in TransactionTableModel::data() into a standalone, Qt-free
//! module so that:
//!
//!   1. The trickiest logic gets unit-test coverage in the GUI-OFF build
//!      configuration that CI actually exercises (the blind spot that hid
//!      PR #2944's heap corruption).
//!   2. The same evaluator is consumed by the producer-side per-view cursors
//!      in later PRs (server-side filter/sort), once the client
//!      QSortFilterProxyModel is retired.
//!
//! Deliberately contains NO Qt headers: it compiles into test_gridcoin with
//! ENABLE_GUI=OFF. The FilterSpec value type is the parameter the GUI pushes
//! down to the producer cursor (Phase 3) and that marshals over IPC unchanged
//! (Phase 4) — value-typed, pointer-free, optimize-for-the-wire.
//!

namespace GRC {

//!
//! \brief Column indices, a standalone mirror of
//! TransactionTableModel::ColumnIndex.
//!
//! Kept here (rather than including the Qt model header) so this translation
//! unit stays Qt-free. transactionfilterproxy.cpp static_asserts these against
//! the authoritative Qt-side enum so they cannot drift.
//!
enum TxSortColumn {
    TXCOL_STATUS  = 0,
    TXCOL_DATE    = 1,
    TXCOL_TYPE    = 2,
    TXCOL_ADDRESS = 3,
    TXCOL_AMOUNT  = 4,
};

//!
//! \brief The two TransactionStatus::Status values the filter treats as
//! "inactive" (conflicted / not accepted).
//!
//! Mirror of the Qt-side enum; transactionfilterproxy.cpp static_asserts these
//! against TransactionStatus::Conflicted / NotAccepted so they cannot drift.
//!
constexpr int TXSTATUS_CONFLICTED  = 6;
constexpr int TXSTATUS_NOTACCEPTED = 9;

//!
//! \brief Sort order, a mirror of Qt::AscendingOrder (0) / Qt::DescendingOrder (1).
//!
enum TxSortOrder {
    TXSORT_ASC  = 0,
    TXSORT_DESC = 1,
};

//!
//! \brief Type-mask helpers — Qt-free replacements for the deleted
//! TransactionFilterProxy::ALL_TYPES / TYPE(). FilterSpec.type_mask is a bit field
//! of (1u << TransactionRecord::Type); ALL_TYPES selects every type.
//!
constexpr uint32_t ALL_TYPES = 0xFFFFFFFF;
constexpr uint32_t TypeBit(int type) { return 1u << type; }

//!
//! \brief Serializable filter specification.
//!
//! The value-typed parameter the view pushes down to the producer-side cursor
//! (Phase 3) and that marshals over IPC unchanged (Phase 4). Defaults reproduce
//! TransactionFilterProxy's initial state: all dates, all types, no address
//! filter, no minimum amount, unlimited rows, show everything including
//! orphans-as-configured.
//!
struct FilterSpec {
    //! Inclusive lower bound, seconds since epoch. Default 0 == MIN_DATE.
    int64_t date_from = 0;
    //! Inclusive upper bound, seconds since epoch. Default 0xFFFFFFFF == MAX_DATE.
    int64_t date_to = 0xFFFFFFFF;
    //! Bit field of (1u << TransactionRecord::Type). Default 0xFFFFFFFF == ALL_TYPES.
    uint32_t type_mask = 0xFFFFFFFF;
    //! Case-insensitive substring matched against the address OR the label.
    //! Empty matches everything.
    std::string address_substr;
    //! Compared against llabs(net_amount): a row is rejected if its absolute
    //! net amount is below this.
    int64_t min_amount = 0;
    //! Maximum rows the view exposes, -1 == unlimited. NOT a per-row predicate
    //! (Accepts() ignores it); applied as a post-filter count cap by the
    //! consumer (today TransactionFilterProxy::rowCount, later the cursor).
    int32_t limit_rows = -1;
    //! Show Conflicted / NotAccepted rows.
    bool show_inactive = true;
    //! The -showorphans launch arg. When false, Conflicted / NotAccepted rows
    //! are masked regardless of show_inactive (mirrors the original two-gate
    //! logic in filterAcceptsRow).
    bool show_orphans = false;
};

//!
//! \brief The per-row fields the filter predicate reads.
//!
//! The producer projects a TransactionRecord (+ address-book label) into this
//! in later PRs; in PR1 TransactionFilterProxy builds it from the model roles
//! it already reads.
//!
struct TxFilterFields {
    //! rec.time, seconds since epoch.
    int64_t time = 0;
    //! rec.credit + rec.debit, SIGNED net amount (Accepts() takes the abs).
    int64_t net_amount = 0;
    //! TransactionRecord::Type.
    int type = 0;
    //! TransactionStatus::Status.
    int status = 0;
    //! rec.address.
    std::string address;
    //! Address-book label for rec.address (may be empty).
    std::string label;
};

//!
//! \brief The per-row keys the sort comparator reads.
//!
//! type_string / address_string are the localized, GUI-formatted strings
//! (formatTxType / formatTxToAddress(..., /*tooltip=*/true)) supplied by the
//! GUI, so this module never localizes — keeping localization GUI-process-side
//! across the eventual multiprocess split. status_sort_key is rec.status.sortKey.
//!
struct SortKey {
    int64_t time = 0;
    int64_t net_amount = 0;
    std::string status_sort_key;
    std::string type_string;
    std::string address_string;
};

//!
//! \brief Exact port of TransactionFilterProxy::filterAcceptsRow.
//!
//! \return true if the row passes the filter and should be shown.
//!
bool Accepts(const TxFilterFields& fields, const FilterSpec& spec);

//!
//! \brief Exact port of the Qt::EditRole sort-key comparison.
//!
//! The proxy sorts on Qt::EditRole with case-insensitive string comparison.
//! Returns true if \p a sorts strictly before \p b for the given \p column
//! and \p order. Equal keys return false (a strict weak ordering).
//!
//! \param column One of TxSortColumn.
//! \param order  One of TxSortOrder.
//!
bool Less(const SortKey& a, const SortKey& b, int column, int order);

} // namespace GRC

#endif // GRIDCOIN_QT_TXFILTER_H
