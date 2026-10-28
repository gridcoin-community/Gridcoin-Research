if(NOT SYSTEM_XXD)
    add_executable(xxd xxd/xxd.c)
    set(XXD $<TARGET_FILE:xxd>)
endif()

add_executable(test_gridcoin
    alt_signal_stack.cpp
    alt_signal_stack_tests.cpp
    bip68_tests.cpp
    block_version_tests.cpp
    checkpoints_tests.cpp
    coinstake_construction_tests.cpp
    csv_tests.cpp
    dos_tests.cpp
    accounting_tests.cpp
    chain_setup.cpp
    state_guard.cpp
    gridcoin/message_contract_tests.cpp
    state_leak_detector.cpp
    state_guard_tests.cpp
    miner_block_assembly_tests.cpp
    # SetBestChain's trust-regression revert leg, driven on the fixture chain.
    chainman_reorg_tests.cpp
    # Which block a side-branch block's staked input comes from, on the fixture chain.
    kernel_branch_lookup_tests.cpp
    # The fixture chain's own clock.
    chain_setup_tests.cpp
    addressbook_tests.cpp
    addrman_tests.cpp
    allocator_tests.cpp
    base32_tests.cpp
    base58_tests.cpp
    base64_tests.cpp
    bip32_tests.cpp
    #compilerbug_tests.cpp
    crypto_tests.cpp
    diagnose_tests.cpp
    fs_tests.cpp
    getarg_tests.cpp
    gridcoin_tests.cpp
    htlc_tests.cpp
    key_io_tests.cpp
    gridcoin/block_finder_tests.cpp
    gridcoin/block_rewards_tests.cpp
    gridcoin/boinc_tests.cpp
    gridcoin/beacon_advertisement_tests.cpp
    gridcoin/beacon_tests.cpp
    gridcoin/claim_tests.cpp
    gridcoin/contract_tests.cpp
    gridcoin/cpid_tests.cpp
    gridcoin/difficulty_tests.cpp
    gridcoin/enumbytes_tests.cpp
    gridcoin/http_tests.cpp
    gridcoin/kernel_tests.cpp
    gridcoin/magnitude_tests.cpp
    gridcoin/mnemonics_tests.cpp
    gridcoin/mrc_tests.cpp
    gridcoin/poll_result_cache_tests.cpp
    gridcoin/poll_registry_tests.cpp
    gridcoin/poll_result_tests.cpp
    gridcoin/pool_chain_tests.cpp
    gridcoin/pool_tests.cpp
    gridcoin/project_tests.cpp
    gridcoin/protocol_tests.cpp
    gridcoin/researcher_tests.cpp
    gridcoin/rsaverify_tests.cpp
    gridcoin/scraper_net_tests.cpp
    gridcoin/scraper_registry_tests.cpp
    gridcoin/scraper_tests.cpp
    gridcoin/seen_stakes_tests.cpp
    gridcoin/sidestake_tests.cpp
    gridcoin/superblock_tests.cpp
    gridcoin/voting_notify_tests.cpp
    interfaces_tests.cpp
    key_tests.cpp
    limitedmap_tests.cpp
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
    # Qt-free grouped index core (src/wallet/coinviews.cpp) of the windowed
    # coin-control selection model (#3183) + its unit suite: flat/tree index
    # maintenance, directory aggregate arithmetic, per-scope deltas, and the
    # scope-local high-water + per-view floor contract exercised against the
    # real WindowCache gate — tested GUI-OFF like the cursor.
    ${CMAKE_SOURCE_DIR}/src/wallet/coinviews.cpp
    qt_coinviews_tests.cpp
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
    # Coin-store worker / double-queue (src/wallet/walletcoinstore.cpp) unit
    # suite (#3183): upsert diffing, suppress-when-unwatched, the validated
    # selection mirror, and the value-filter parity semantics. Core lives in
    # gridcoin_util, so only the suite is listed here (GUI-OFF).
    qt_walletcoinstore_tests.cpp
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
    # CTxDB batch-consistent reads: Exists() against a pending Delete.
    dbwrapper_tests.cpp
    random_tests.cpp
    rpc_tests.cpp
    rpchelpman_tests.cpp
    sanity_tests.cpp
    scheduler_tests.cpp
    script_p2sh_tests.cpp
    gridcoin/connectinputs_tests.cpp
    rpc_allowlist_tests.cpp
    script_tests.cpp
    scrypt_tests.cpp
    serialize_tests.cpp
    sigopcount_tests.cpp
    sync_tests.cpp
    test_gridcoin.cpp
    timedata_tests.cpp
    transaction_identity_tests.cpp
    transaction_tests.cpp
    txmempool_tests.cpp
    uint256_tests.cpp
    util_fraction_tests.cpp
    util_tests.cpp
    util_threadnames_tests.cpp
    wallet_db_tests.cpp
    wallet_tests.cpp
)

add_subdirectory(data)

target_include_directories(test_gridcoin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
)

# Boost.Test's headers are compiled as user headers, never as system headers.
#
# BOOST_CHECK_EQUAL(a, b) compares a with b inside Boost.Test's equal_impl
# (boost/test/tools/old/impl.hpp). Where that header is a system header, the
# compiler drops the -Wsign-compare it raises there, although the mixed types
# are ours. Only the Intel macOS job ever showed these, and only because
# Homebrew's include directory reached the compiler as a user path. GCC and
# Clang both give a header the system status of the header that included it,
# so the whole boost/test tree has to be found through a user include
# directory, from each test's own #include down. A link to it in the build
# tree, searched ahead of every other directory, does that with either
# compiler and wherever Boost is installed. The rest of Boost stays a system
# header.
#
# Boost.Test's own declarations then raise warnings of their own, which are
# Boost's code. boost_test_prelude.h, included ahead of every source here,
# silences those and only those.
#
# The headers are looked for first in the include directories of
# Boost::unit_test_framework, the library test_gridcoin links, so that where
# more than one Boost is installed they come from the same one.
set(_boost_test_candidates "")
foreach(_boost_target Boost::unit_test_framework Boost::headers Boost::boost)
    if(TARGET ${_boost_target})
        get_target_property(_boost_dirs ${_boost_target} INTERFACE_INCLUDE_DIRECTORIES)
        if(_boost_dirs)
            list(APPEND _boost_test_candidates ${_boost_dirs})
        endif()
    endif()
endforeach()
list(APPEND _boost_test_candidates ${Boost_INCLUDE_DIRS})

set(_boost_test_root "")
foreach(_boost_dir IN LISTS _boost_test_candidates)
    if(EXISTS "${_boost_dir}/boost/test/unit_test.hpp")
        set(_boost_test_root "${_boost_dir}")
        break()
    endif()
endforeach()
if(NOT _boost_test_root)
    message(FATAL_ERROR "boost/test/unit_test.hpp is not under any Boost include directory: ${_boost_test_candidates}")
endif()

set(BOOST_TEST_USER_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/boost_test_user")
set(_boost_test_dest "${BOOST_TEST_USER_INCLUDE_DIR}/boost/test")
file(MAKE_DIRECTORY "${BOOST_TEST_USER_INCLUDE_DIR}/boost")
# CREATE_LINK replaces an earlier link but fails on a directory, which is what a
# copy made by an earlier configure is. That copy is removed first, so the
# headers always come from this configure's Boost. A link is never passed to
# REMOVE_RECURSE.
if(EXISTS "${_boost_test_dest}" AND NOT IS_SYMLINK "${_boost_test_dest}")
    file(REMOVE_RECURSE "${_boost_test_dest}")
endif()
file(CREATE_LINK "${_boost_test_root}/boost/test" "${_boost_test_dest}"
    RESULT _boost_test_link SYMBOLIC)
if(NOT _boost_test_link STREQUAL "0")
    # A host that cannot make the link gets a copy instead.
    file(COPY "${_boost_test_root}/boost/test" DESTINATION "${BOOST_TEST_USER_INCLUDE_DIR}/boost")
endif()
message(STATUS "Boost.Test headers compiled as user headers, from ${_boost_test_root}/boost/test")

target_include_directories(test_gridcoin BEFORE PRIVATE ${BOOST_TEST_USER_INCLUDE_DIR})
# Joined to its argument so that it stays one option. GCC and Clang take
# -include<file>; clang-cl, which the top-level MSVC check lets through, takes
# /FI<file>.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(_boost_test_prelude "/FI${CMAKE_CURRENT_SOURCE_DIR}/boost_test_prelude.h")
else()
    set(_boost_test_prelude "-include${CMAKE_CURRENT_SOURCE_DIR}/boost_test_prelude.h")
endif()
target_compile_options(test_gridcoin PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${_boost_test_prelude}>")

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
    Boost::boost Boost::filesystem Boost::iostreams Boost::thread Boost::date_time Boost::unit_test_framework
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

# BOTH halves are required on musl; neither is sufficient alone.
#
# alt_signal_stack.cpp installs an adequately sized alternate stack during static
# initialisation, which is what covers framework::init() -- the switch below is
# applied only in unit_test_monitor_t::execute_and_translate and so cannot reach
# the init path, which is where the abort actually happens.
#
# But Boost's signal_handler DESTRUCTOR disables the alternate stack
# unconditionally, ours included, so the pre-install protects exactly the first
# monitored scope. Every later test case would get Boost's own SIGSTKSZ-sized
# install -- 8192 on musl -- which is the switch's job to suppress.
#
# Verified by strace: the pre-install alone leaves Boost installing per test case;
# the switch alone leaves framework::init() installing. Together, neither happens.
add_test(NAME gridcoin_tests COMMAND test_gridcoin --use_alt_stack=no)
