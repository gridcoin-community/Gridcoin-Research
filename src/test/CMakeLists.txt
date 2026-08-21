if(NOT SYSTEM_XXD)
    add_executable(xxd xxd/xxd.c)
    set(XXD $<TARGET_FILE:xxd>)
endif()

add_executable(test_gridcoin
    bip68_tests.cpp
    block_version_tests.cpp
    checkpoints_tests.cpp
    coinstake_construction_tests.cpp
    csv_tests.cpp
    dos_tests.cpp
    accounting_tests.cpp
    addressbook_tests.cpp
    addrman_tests.cpp
    allocator_tests.cpp
    base32_tests.cpp
    base58_tests.cpp
    base64_tests.cpp
    bip32_tests.cpp
    #compilerbug_tests.cpp
    crypto_tests.cpp
    fs_tests.cpp
    getarg_tests.cpp
    gridcoin_tests.cpp
    htlc_tests.cpp
    gridcoin/block_finder_tests.cpp
    gridcoin/block_rewards_tests.cpp
    gridcoin/boinc_tests.cpp
    gridcoin/beacon_tests.cpp
    gridcoin/claim_tests.cpp
    gridcoin/contract_tests.cpp
    gridcoin/cpid_tests.cpp
    gridcoin/enumbytes_tests.cpp
    gridcoin/magnitude_tests.cpp
    gridcoin/mnemonics_tests.cpp
    gridcoin/mrc_tests.cpp
    gridcoin/poll_result_cache_tests.cpp
    gridcoin/pool_tests.cpp
    gridcoin/project_tests.cpp
    gridcoin/protocol_tests.cpp
    gridcoin/researcher_tests.cpp
    gridcoin/rsaverify_tests.cpp
    gridcoin/scraper_registry_tests.cpp
    gridcoin/sidestake_tests.cpp
    gridcoin/superblock_tests.cpp
    interfaces_tests.cpp
    key_tests.cpp
    merkle_tests.cpp
    merkleblock_tests.cpp
    mruset_tests.cpp
    multisig_tests.cpp
    netbase_tests.cpp
    net_tests.cpp
    orphan_block_tests.cpp
    psgt_pool_tests.cpp
    psgt_tests.cpp
    # Qt-free transaction filter/sort core (src/qt/txfilter.cpp) + its unit
    # suite. The core has zero Qt dependencies, so it compiles into the
    # GUI-OFF test binary directly — landing the GUI predicate logic in the
    # one configuration CI exercises.
    ${CMAKE_SOURCE_DIR}/src/wallet/txfilter.cpp
    qt_txfilter_tests.cpp
    # Qt-free transaction ordering core (src/qt/txorder.cpp) + its unit suite:
    # the position/clustering logic the producer-side WalletTxStore relies on,
    # tested in the GUI-OFF configuration CI exercises.
    ${CMAKE_SOURCE_DIR}/src/wallet/txorder.cpp
    qt_txorder_tests.cpp
    # Qt-free per-view cursor (src/qt/cursor.cpp) + its unit suite: the
    # view_index-maintenance arithmetic (windowed-model PR3), tested GUI-OFF.
    ${CMAKE_SOURCE_DIR}/src/wallet/cursor.cpp
    qt_cursor_tests.cpp
    # Qt-free windowed-table reconciliation core (src/qt/windowcache.h, a
    # header-only template — no .cpp) + its unit suite: the consumer-side
    # two-channel split (structural deltas vs scroll-driven content fetch) and the
    # cache-base arithmetic (windowed-model PR5), tested GUI-OFF.
    qt_windowcache_tests.cpp
    # Wallet-transaction DTOs (src/qt/transactionrecord.cpp), Qt-scrubbed for
    # the multiprocess boundary (Phase 1c-ii) + smoke suite. Compiling the TU
    # into the GUI-OFF binary pins the Qt-freedom the node-side source needs.
    ${CMAKE_SOURCE_DIR}/src/wallet/transactionrecord.cpp
    qt_transactionrecord_tests.cpp
    # Qt-free wallet event queue (src/wallet/wallet_event_queue.h, a header-only
    # template shared by the tx and coin channels) unit suite: the MPSC intake
    # drain/ordering + multi-producer seqno-density contract (GUI-OFF).
    qt_wallet_event_queue_tests.cpp
    # Qt-free store-worker / double-queue (src/wallet/wallettxstore.cpp) unit
    # suite: the worker's drain/ordering/concurrency + clean shutdown and the
    # null-wallet-safe getRowDetail early-returns. Core lives in gridcoin_util,
    # so only the suite is listed here (GUI-OFF).
    qt_wallettxstore_tests.cpp
    # The WALLET-BACKED half of the same store: applyChainTipRefresh() and prime()
    # need a live CWallet plus a chain tip, so the null-wallet suite above cannot
    # reach them at all. This one drives the real coinstake NotAccepted -> flip-in
    # path against pwalletMain and a mock chain (#3257).
    qt_wallettxstore_chain_tests.cpp
    # Transport helpers (src/ipc/process.cpp). Also Cap'n Proto-free. Needed here
    # because on WIN32 handshake.cpp's WriteFileAtomic calls MakeOwnerOnlyDacl /
    # ApplyOwnerOnlyDacl (defined at process.cpp:587,607) to create the cookie with
    # an owner-only DACL. gridcoin_ipc would supply them, but it is deliberately
    # NOT linked here on Windows (see the ENABLE_MULTIPROCESS AND NOT WIN32 guard
    # below), so without this the win64 test link fails on those two symbols while
    # every other target succeeds.
    ${CMAKE_SOURCE_DIR}/src/ipc/process.cpp
    # Multiprocess connect-handshake logic (src/ipc/handshake.cpp): identity-token
    # hashing, the ClientHandshake matching policy, and CheckIdentityBinding. The
    # TU has no Cap'n Proto / libmultiprocess dependency, so it compiles into the
    # ordinary (non-multiprocess) test binary directly -- these unit tests run in
    # every CI config, not just the multiprocess job.
    ${CMAKE_SOURCE_DIR}/src/ipc/handshake.cpp
    ipc_handshake_tests.cpp
    # Serve-side handshake gating (src/ipc/serve_init.cpp): ServeInit refuses to
    # serve anything until authenticate() succeeds. Also Cap'n Proto-free, so it
    # compiles into the ordinary test binary alongside handshake.cpp.
    ${CMAKE_SOURCE_DIR}/src/ipc/serve_init.cpp
    ipc_serve_init_tests.cpp
    # Peer-credential check (src/ipc/peercred.cpp): SO_PEERCRED / getpeereid
    # same-user enforcement. No Cap'n Proto, so it compiles into the ordinary
    # test binary alongside the sources above.
    ${CMAKE_SOURCE_DIR}/src/ipc/peercred.cpp
    ipc_peercred_tests.cpp
    random_tests.cpp
    rpc_tests.cpp
    rpchelpman_tests.cpp
    sanity_tests.cpp
    scheduler_tests.cpp
    script_p2sh_tests.cpp
    script_tests.cpp
    serialize_tests.cpp
    sigopcount_tests.cpp
    sync_tests.cpp
    test_gridcoin.cpp
    transaction_identity_tests.cpp
    transaction_tests.cpp
    txmempool_tests.cpp
    uint256_tests.cpp
    util_tests.cpp
    wallet_tests.cpp
)

add_subdirectory(data)

target_include_directories(test_gridcoin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
)

# Handle Boost Test linking mode automatically
if(NOT STATIC_LIBS)
    # Native Build: Linking against shared .so libraries requires this flag
    target_compile_definitions(test_gridcoin PRIVATE BOOST_TEST_DYN_LINK)
else()
    # Depends Build: Linking against static .a libraries requires this flag (or simply omitting DYN_LINK)
    target_compile_definitions(test_gridcoin PRIVATE BOOST_TEST_STATIC_LINK)
endif()

set_property(SOURCE test_gridcoin.cpp APPEND PROPERTY
    COMPILE_DEFINITIONS "BOOST_TEST_MODULE=Gridcoin Test Suite"
)

target_link_libraries(test_gridcoin PRIVATE
    ${RUNTIME_LIBS}
    Boost::unit_test_framework
    gridcoin_util

    # Dependencies of gridcoin_util
    ${LIBBDB_CXX} ${LIBLEVELDB} ${LIBSECP256K1} ${LIBUNIVALUE}
    Boost::boost Boost::filesystem Boost::iostreams Boost::thread Boost::date_time Boost::serialization Boost::unit_test_framework
    CURL::libcurl
    OpenSSL::Crypto OpenSSL::SSL
    Threads::Threads
    ZLIB::ZLIB
)

# The IPC sources compiled in above use the Win32 DACL API (SetEntriesInAclW,
# Get/SetNamedSecurityInfoW, InitializeSecurityDescriptor,
# SetSecurityDescriptorDacl). gridcoin_ipc carries advapi32 for the other
# targets, but it is not linked here on Windows, so name it directly.
if(WIN32)
    target_link_libraries(test_gridcoin PRIVATE advapi32)
endif()

# Wire-level IPC handshake test (ipc_wire_handshake_tests.cpp): drives
# ClientHandshake against a served ServeInit over a REAL AF_UNIX Cap'n Proto
# connection. It needs the actual transport (gridcoin_ipc + the generated capnp
# proxies), so it only builds under ENABLE_MULTIPROCESS. Its harness is POSIX-only
# (mkdtemp / sockaddr_un / unistd), so it is additionally gated to non-Windows --
# ENABLE_MULTIPROCESS also covers the win64 MP port, which lacks those APIs; the
# handshake logic itself is covered cross-platform by the capnp-free ipc suites
# and validated live on Windows. Linking gridcoin_ipc propagates C++20 and the
# capnp/kj headers to test_gridcoin in multiprocess builds.
if(ENABLE_MULTIPROCESS AND NOT WIN32)
    target_sources(test_gridcoin PRIVATE ipc_wire_handshake_tests.cpp)
    target_link_libraries(test_gridcoin PRIVATE gridcoin_ipc)
endif()

add_test(NAME gridcoin_tests COMMAND test_gridcoin)
