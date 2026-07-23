# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xdd5d967b6b9f5e26;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/handler.h");
$Proxy.includeTypes("ipc/capnp/handler-types.h");

interface Handler $Proxy.wrap("interfaces::Handler") {
    destroy @0 (context :Proxy.Context) -> ();
    disconnect @1 (context :Proxy.Context) -> ();
}
