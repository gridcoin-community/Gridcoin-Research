// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "txfilter.h"

#include <cstdlib>

namespace {

//! ASCII case fold. The original predicates use Qt::CaseInsensitive, which for
//! the inputs that actually occur here — base58 addresses and ASCII labels —
//! is exactly ASCII case folding. Non-ASCII bytes pass through unchanged; the
//! only divergence from Qt's Unicode-aware folding is case-insensitive
//! matching of non-ASCII characters in a label substring search, which is
//! sub-cosmetic. (Sort comparison via Less() is not yet wired to the live
//! sort in PR1 — the proxy still sorts via Qt EditRole — so any collation
//! nuance there is exercised only by unit tests until a later PR.)
std::string AsciiLower(const std::string& s)
{
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

//! Case-insensitive substring test. Mirrors QString::contains(needle,
//! Qt::CaseInsensitive): an empty needle matches everything.
bool IContains(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) return true;
    return AsciiLower(haystack).find(AsciiLower(needle)) != std::string::npos;
}

//! Case-insensitive lexicographic compare; sign matches std::string::compare.
int ICompare(const std::string& a, const std::string& b)
{
    return AsciiLower(a).compare(AsciiLower(b));
}

} // anonymous namespace

namespace GRC {

bool Accepts(const TxFilterFields& f, const FilterSpec& s)
{
    // Mirrors TransactionFilterProxy::filterAcceptsRow (transactionfilterproxy.cpp).
    // Two separate gates reject inactive rows, kept 1:1 with the original:
    //   (1) the explicit show-inactive toggle, and
    //   (2) the -showorphans launch-arg mask.
    const bool inactive = (f.status == TXSTATUS_CONFLICTED || f.status == TXSTATUS_NOTACCEPTED);

    if (!s.show_inactive && inactive) return false;
    if (!s.show_orphans && inactive) return false;

    if (!((1u << f.type) & s.type_mask)) return false;

    if (f.time < s.date_from || f.time > s.date_to) return false;

    if (!IContains(f.address, s.address_substr) && !IContains(f.label, s.address_substr)) {
        return false;
    }

    if (std::llabs(f.net_amount) < s.min_amount) return false;

    return true;
}

bool Less(const SortKey& a, const SortKey& b, int column, int order)
{
    // Three-way compare in ascending sense (<0 a before b, 0 equal, >0 after).
    int cmp = 0;
    switch (column) {
    case TXCOL_DATE:
        cmp = (a.time < b.time) ? -1 : (a.time > b.time ? 1 : 0);
        break;
    case TXCOL_AMOUNT:
        cmp = (a.net_amount < b.net_amount) ? -1 : (a.net_amount > b.net_amount ? 1 : 0);
        break;
    case TXCOL_STATUS:
        cmp = ICompare(a.status_sort_key, b.status_sort_key);
        break;
    case TXCOL_TYPE:
        cmp = ICompare(a.type_string, b.type_string);
        break;
    case TXCOL_ADDRESS:
        cmp = ICompare(a.address_string, b.address_string);
        break;
    default:
        cmp = 0;
        break;
    }

    // Strict-less in the requested order sense; equal keys -> false.
    return (order == TXSORT_DESC) ? (cmp > 0) : (cmp < 0);
}

} // namespace GRC
