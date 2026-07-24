# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

# Multiprocess (IPC) layer for the interfaces:: boundary (RFC #2937, Phase 2).
# Reached only when ENABLE_MULTIPROCESS is ON (this directory is add_subdirectory'd
# from src/CMakeLists.txt under that guard).

# Build the vendored libmultiprocess runtime + mpgen from the in-tree subtree,
# unless an external libmultiprocess was requested (then the top-level
# CMakeLists.txt found it via find_package).
if(NOT WITH_EXTERNAL_LIBMULTIPROCESS)
    include(${CMAKE_SOURCE_DIR}/cmake/libmultiprocess.cmake)
    add_libmultiprocess(libmultiprocess)
endif()

# The IPC layer: the hand-written transport (process/protocol/interfaces) plus the
# generated Cap'n Proto proxy layer. target_capnp_sources runs mpgen on each
# .capnp, generating the raw capnp types (.capnp.h/.c++) and the ProxyClient/
# ProxyServer marshalling code (.proxy-*), and adds them as sources of this
# library. The include prefix (this dir) reproduces the ipc/capnp/... include
# spelling the schemas' $Proxy.includeTypes and consumers use.
add_library(gridcoin_ipc STATIC EXCLUDE_FROM_ALL
    process.cpp
    interfaces.cpp
    handshake.cpp
    serve_init.cpp
    connect.cpp
    capnp/protocol.cpp
)

target_capnp_sources(gridcoin_ipc ${CMAKE_CURRENT_SOURCE_DIR}
    capnp/handler.capnp
    capnp/staking.capnp
    capnp/node.capnp
    capnp/wallet.capnp
    capnp/wallet_tx_source.capnp
    capnp/mrc.capnp
    capnp/voting.capnp
    capnp/researcher.capnp
    capnp/init.capnp
)

# ${CMAKE_SOURCE_DIR}/src resolves the interfaces/*.h the generated code wraps;
# ${CMAKE_BINARY_DIR}/src resolves the generated ipc/capnp/*.capnp.h headers
# (matching the project-wide convention in this file's parent).
target_include_directories(gridcoin_ipc PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
)

# The generated proxy code includes libmultiprocess's mp/ headers, which use C++20
# concepts; the rest of Gridcoin is C++17. Compile this target (and propagate to
# anything linking it, since the proxy headers are C++20) as C++20.
set_target_properties(gridcoin_ipc PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_features(gridcoin_ipc PUBLIC cxx_std_20)

# Link gridcoin_util for the core interface headers' include environment (the
# interfaces:: headers the generated proxies wrap transitively include core
# headers such as univalue). Static-lib linkage here only propagates include
# dirs / compile definitions; no objects are pulled into libgridcoin_ipc.a.
target_link_libraries(gridcoin_ipc PUBLIC
    gridcoin_util
)
