// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_INTERFACES_MARSHAL_H
#define GRIDCOIN_INTERFACES_MARSHAL_H

#include <type_traits>

//! Compile-time pin (multiprocess design §4.1) that a value type crossing the
//! interfaces:: boundary stays freely copyable, so a node-side implementation can
//! fill one and ship it across the boundary. It trips on a regression that adds a
//! move-only / non-copyable member -- a std::unique_ptr, a std::mutex/atomic, or
//! a reference. It does NOT (and cannot, via a copy-constructibility trait) catch
//! a raw pointer into core state or a Qt type, both of which are still copyable:
//! keeping DTOs pointer-free and Qt-free remains a review-owned discipline (and
//! the doc comment on each DTO). Placed next to each DTO group in the interface
//! headers; see wallet_tx_record.h for the additional fixed-width field pins.
#define INTERFACES_ASSERT_MARSHALABLE(T)                            \
    static_assert(std::is_copy_constructible_v<T>                   \
                      && std::is_copy_assignable_v<T>               \
                      && std::is_move_constructible_v<T>,           \
                  #T " must remain a copyable value type for the "  \
                     "interfaces:: boundary")

#endif // GRIDCOIN_INTERFACES_MARSHAL_H
