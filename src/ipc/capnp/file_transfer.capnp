# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

@0x8656a40b98ebf945;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("ipc::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
$Proxy.include("interfaces/file_transfer.h");
$Proxy.includeTypes("ipc/capnp/file_transfer-types.h");

# Mirrors interfaces::FileSource and interfaces::FileSink
# (src/interfaces/file_transfer.h), the two ends of the file-transfer channel.
# Both are pure-abstract, so every method is schema'd (else the generated
# ProxyClient would stay abstract). A client reaches them only through the
# Wallet methods that return them. uint256 (the SHA-256 in the trailer and in
# finish) crosses as Data via common-types.h; the chunk bytes cross as Data.
interface FileSource $Proxy.wrap("interfaces::FileSource") {
    destroy @0 (context :Proxy.Context) -> ();
    entries @1 (context :Proxy.Context) -> (result :List(FileEntryInfo));
    read @2 (context :Proxy.Context, entry :UInt32, maxBytes :UInt32) -> (result :FileChunk);
    trailer @3 (context :Proxy.Context, entry :UInt32) -> (result :FileTrailer);
    cancel @4 (context :Proxy.Context) -> ();
}

interface FileSink $Proxy.wrap("interfaces::FileSink") {
    destroy @0 (context :Proxy.Context) -> ();
    write @1 (context :Proxy.Context, data :Data) -> (result :Bool);
    finish @2 (context :Proxy.Context, size :UInt64, sha256 :Data) -> (result :FileSinkResult);
    cancel @3 (context :Proxy.Context) -> ();
}

struct FileEntryInfo $Proxy.wrap("interfaces::FileEntryInfo") {
    name @0 :Text;
    size @1 :Int64;
    sizeHint @2 :UInt64 $Proxy.name("size_hint");
    note @3 :Text;
}

struct FileChunk $Proxy.wrap("interfaces::FileChunk") {
    data @0 :Data;
    eof @1 :Bool;
    error @2 :Text;
}

struct FileTrailer $Proxy.wrap("interfaces::FileTrailer") {
    ok @0 :Bool;
    size @1 :UInt64;
    sha256 @2 :Data;
    error @3 :Text;
}

struct FileSinkResult $Proxy.wrap("interfaces::FileSinkResult") {
    ok @0 :Bool;
    output @1 :Text;
    error @2 :Text;
}
