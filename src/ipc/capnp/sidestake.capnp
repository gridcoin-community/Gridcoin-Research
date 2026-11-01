# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0xd57c7ee6dc2deb58;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/sidestake.h");
$Proxy.includeTypes("ipc/capnp/sidestake-types.h");

using Handler = import "handler.capnp";
using Node = import "node.capnp";

# Mirrors interfaces::SideStakeManager (src/interfaces/sidestake.h), the local
# sidestake registry boundary. SideStakeEditStatus crosses as its Int32 value
# (libmultiprocess marshals the enum class as its underlying int). Both handlers
# reuse Node.VoidCallback (ProxyCallback<std::function<void()>>) so the schema
# does not register duplicate capnp interfaces for the same C++ type.
interface SideStakeManager $Proxy.wrap("interfaces::SideStakeManager") {
    destroy @0 (context :Proxy.Context) -> ();

    entries @1 (context :Proxy.Context) -> (result :SideStakeSnapshot);
    localRevision @2 (context :Proxy.Context) -> (result :UInt64);
    addLocal @3 (context :Proxy.Context, address :Text, allocationPercent :Float64, description :Text) -> (result :SideStakeEditResult);
    setAllocation @4 (context :Proxy.Context, address :Text, allocationPercent :Float64) -> (result :SideStakeEditResult);
    setDescription @5 (context :Proxy.Context, address :Text, description :Text) -> (result :SideStakeEditResult);
    deleteLocal @6 (context :Proxy.Context, address :Text) -> (result :SideStakeEditResult);
    handleRwSettingsUpdated @7 (context :Proxy.Context, callback :Node.VoidCallback) -> (result :Handler.Handler);
    handleMandatorySideStakeChanged @8 (context :Proxy.Context, callback :Node.VoidCallback) -> (result :Handler.Handler);
}

struct SideStakeEntry $Proxy.wrap("interfaces::SideStakeEntry") {
    address @0 :Text;
    allocationPercent @1 :Float64 $Proxy.name("allocation_percent");
    description @2 :Text;
    status @3 :Text;
    isMandatory @4 :Bool $Proxy.name("is_mandatory");
    statusSortKey @5 :Int32 $Proxy.name("status_sort_key");
}

struct SideStakeSnapshot $Proxy.wrap("interfaces::SideStakeSnapshot") {
    entries @0 :List(SideStakeEntry);
    localRevision @1 :UInt64 $Proxy.name("local_revision");
}

struct SideStakeEditResult $Proxy.wrap("interfaces::SideStakeEditResult") {
    status @0 :Int32;
    address @1 :Text;
    localRevision @2 :UInt64 $Proxy.name("local_revision");
}
