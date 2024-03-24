if(NOT SYSTEM_XXD)
    add_executable(xxd xxd/xxd.c)
    set(XXD $<TARGET_FILE:xxd>)
endif()

add_executable(test_gridcoin
    checkpoints_tests.cpp
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
    gridcoin/block_finder_tests.cpp
    gridcoin/beacon_tests.cpp
    gridcoin/claim_tests.cpp
    gridcoin/contract_tests.cpp
    gridcoin/cpid_tests.cpp
    gridcoin/enumbytes_tests.cpp
    gridcoin/magnitude_tests.cpp
    gridcoin/mrc_tests.cpp
    gridcoin/project_tests.cpp
    gridcoin/protocol_tests.cpp
    gridcoin/researcher_tests.cpp
    gridcoin/scraper_registry_tests.cpp
    gridcoin/sidestake_tests.cpp
    gridcoin/superblock_tests.cpp
    key_tests.cpp
    merkle_tests.cpp
    mruset_tests.cpp
    multisig_tests.cpp
    netbase_tests.cpp
    net_tests.cpp
    random_tests.cpp
    rpc_tests.cpp
    sanity_tests.cpp
    scheduler_tests.cpp
    script_p2sh_tests.cpp
    script_tests.cpp
    serialize_tests.cpp
    sigopcount_tests.cpp
    sync_tests.cpp
    test_gridcoin.cpp
    transaction_tests.cpp
    uint256_tests.cpp
    util_tests.cpp
    wallet_tests.cpp
)

add_subdirectory(data)

target_link_libraries(test_gridcoin PRIVATE
    ${RUNTIME_LIBS}
    Boost::unit_test_framework
    gridcoin_util
)

add_test(NAME gridcoin_tests COMMAND test_gridcoin)
