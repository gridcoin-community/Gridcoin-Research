// Copyright (c) 2026 The Gridcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_IPC_CAPNP_TYPE_VARIANT_H
#define GRIDCOIN_IPC_CAPNP_TYPE_VARIANT_H

#include <mp/proxy-types.h>
#include <mp/util.h>

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>
#include <variant>

//! Marshalling for std::variant<Ts...>. libmultiprocess has no Cap'n Proto
//! union support (its Accessor machinery drives plain struct fields only), so a
//! variant crosses as a "discriminated struct": a UInt16 `which` field holding
//! the active alternative's zero-based index, followed by one field per
//! alternative in declaration order, of which only the active one is populated.
//!
//! The paired Cap'n Proto struct MUST be declared as
//!
//!   struct FooWire { which @0 :UInt16; alt0 @1 :T0; alt1 @2 :T1; ... }
//!
//! with `which` first and the alternatives in the SAME order as the C++
//! std::variant. It is a synthetic wire type, so it carries no $Proxy.wrap.
//!
//! This is the variant analogue of mp/type-pair.h and reaches into the
//! generated ProxyStruct Accessors exactly as that header does.
namespace mp {

//! Build the active alternative into wire field (index + 1). Walks the
//! alternatives at compile time and stops at the runtime-active one.
template <std::size_t I, typename Accessors, typename Wire, typename Variant>
void BuildVariantAlt(InvokeContext& invoke_context, Wire& wire, const Variant& value)
{
    if constexpr (I < std::variant_size_v<Variant>) {
        if (value.index() == I) {
            BuildField(TypeList<std::variant_alternative_t<I, Variant>>(), invoke_context,
                Make<StructField, std::tuple_element_t<I + 1, Accessors>>(wire), std::get<I>(value));
        } else {
            BuildVariantAlt<I + 1, Accessors>(invoke_context, wire, value);
        }
    }
}

template <typename... Alts, typename Value, typename Output>
void CustomBuildField(TypeList<std::variant<Alts...>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Value&& value,
    Output&& output)
{
    auto wire = output.init();
    using Accessors = typename ProxyStruct<typename decltype(wire)::Builds>::Accessors;
    BuildField(TypeList<std::uint16_t>(), invoke_context,
        Make<StructField, std::tuple_element_t<0, Accessors>>(wire),
        static_cast<std::uint16_t>(value.index()));
    BuildVariantAlt<0, Accessors>(invoke_context, wire, value);
}

//! Read wire field (which + 1) into the variant, emplacing the matching
//! alternative. `emplace` takes an std::integral_constant<size_t, I> tag plus
//! the read-constructed arguments and installs alternative I.
template <std::size_t I, typename Variant, typename Accessors, typename Wire, typename EmplaceFn>
void ReadVariantAlt(InvokeContext& invoke_context, const Wire& wire, std::uint16_t which, EmplaceFn&& emplace)
{
    if constexpr (I < std::variant_size_v<Variant>) {
        if (which == I) {
            using Alt = std::variant_alternative_t<I, Variant>;
            ReadField(TypeList<Alt>(), invoke_context,
                Make<StructField, std::tuple_element_t<I + 1, Accessors>>(wire),
                ReadDestEmplace(TypeList<Alt>(), [&](auto&&... args) -> auto& {
                    return emplace(std::integral_constant<std::size_t, I>{}, std::forward<decltype(args)>(args)...);
                }));
        } else {
            ReadVariantAlt<I + 1, Variant, Accessors>(invoke_context, wire, which, std::forward<EmplaceFn>(emplace));
        }
    }
}

template <typename... Alts, typename Input, typename ReadDest>
decltype(auto) CustomReadField(TypeList<std::variant<Alts...>>,
    Priority<1>,
    InvokeContext& invoke_context,
    Input&& input,
    ReadDest&& read_dest)
{
    using Variant = std::variant<Alts...>;
    const auto& wire = input.get();
    using Accessors = typename ProxyStruct<typename Decay<decltype(wire)>::Reads>::Accessors;
    std::uint16_t which = 0;
    ReadField(TypeList<std::uint16_t>(), invoke_context,
        Make<StructField, std::tuple_element_t<0, Accessors>>(wire), ReadDestUpdate(which));
    return read_dest.update([&](Variant& value) {
        ReadVariantAlt<0, Variant, Accessors>(invoke_context, wire, which,
            [&](auto index_tag, auto&&... args) -> auto& {
                constexpr std::size_t Idx = decltype(index_tag)::value;
                value.template emplace<Idx>(std::forward<decltype(args)>(args)...);
                return std::get<Idx>(value);
            });
    });
}

} // namespace mp

#endif // GRIDCOIN_IPC_CAPNP_TYPE_VARIANT_H
