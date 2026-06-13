if(NOT SYSTEM_XXD)
    add_executable(xxd xxd/xxd.c)
    set(XXD $<TARGET_FILE:xxd>)
endif()

add_executable(test_gridcoin
    bip68_tests.cpp
    checkpoints_tests.cpp
    coinstake_construction_tests.cpp
    csv_tests.cpp
    dos_tests.cpp
    accounting_tests.cpp
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
    gridcoin/mrc_tests.cpp
    gridcoin/pool_tests.cpp
    gridcoin/project_tests.cpp
    gridcoin/protocol_tests.cpp
    gridcoin/researcher_tests.cpp
    gridcoin/rsaverify_tests.cpp
    gridcoin/scraper_registry_tests.cpp
    gridcoin/sidestake_tests.cpp
    gridcoin/superblock_tests.cpp
    key_tests.cpp
    merkle_tests.cpp
    mruset_tests.cpp
    multisig_tests.cpp
    netbase_tests.cpp
    net_tests.cpp
    orphan_block_tests.cpp
    psgt_tests.cpp
    # Qt-free transaction filter/sort core (src/qt/txfilter.cpp) + its unit
    # suite. The core has zero Qt dependencies, so it compiles into the
    # GUI-OFF test binary directly — landing the GUI predicate logic in the
    # one configuration CI exercises.
    ${CMAKE_SOURCE_DIR}/src/qt/txfilter.cpp
    qt_txfilter_tests.cpp
    # Qt-free transaction ordering core (src/qt/txorder.cpp) + its unit suite:
    # the position/clustering logic the producer-side WalletTxStore relies on,
    # tested in the GUI-OFF configuration CI exercises.
    ${CMAKE_SOURCE_DIR}/src/qt/txorder.cpp
    qt_txorder_tests.cpp
    # Qt-free per-view cursor (src/qt/cursor.cpp) + its unit suite: the
    # view_index-maintenance arithmetic (windowed-model PR3), tested GUI-OFF.
    ${CMAKE_SOURCE_DIR}/src/qt/cursor.cpp
    qt_cursor_tests.cpp
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
    libzip::zip
    ZLIB::ZLIB
)

add_test(NAME gridcoin_tests COMMAND test_gridcoin)
