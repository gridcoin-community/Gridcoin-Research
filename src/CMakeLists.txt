configure_file(
    config/gridcoin-config.h.in
    config/gridcoin-config.h
)
configure_file(
    obj/build.h.in
    obj/build.h
)

add_compile_definitions(
    BOOST_SPIRIT_THREADSAFE
    __STDC_FORMAT_MACROS
)

if(WIN32)
    add_compile_definitions(_MT _WINDOWS _WIN32_WINNT=0x0601)
    add_compile_definitions(WIN32 BOOST_THREAD_USE_LIB UNICODE)
elseif(UNIX AND NOT APPLE)
    add_compile_definitions(_FILE_OFFSET_BITS=64)
endif()


# Dependencies
# ============

if(SYSTEM_UNIVALUE)
    set(LIBUNIVALUE PkgConfig::UNIVALUE)
else()
    add_subdirectory(univalue)
    set(LIBUNIVALUE univalue)
endif()


# libgridcoin_crypto
# ==================

add_subdirectory(crypto)


# libgridcoin_util
# ================

add_library(gridcoin_util STATIC
    addrdb.cpp
    addrman.cpp
    alert.cpp
    arith_uint256.cpp
    banman.cpp
    base58.cpp
    chainparams.cpp
    chainparamsbase.cpp
    checkpoints.cpp
    clientversion.cpp
    consensus/merkle.cpp
    consensus/tx_verify.cpp
    crypter.cpp
    dbwrapper.cpp
    fs.cpp
    gridcoin/appcache.cpp
    gridcoin/backup.cpp
    gridcoin/beacon.cpp
    gridcoin/boinc.cpp
    gridcoin/claim.cpp
    gridcoin/contract/contract.cpp
    gridcoin/contract/message.cpp
    gridcoin/cpid.cpp
    gridcoin/gridcoin.cpp
    gridcoin/mrc.cpp
    gridcoin/project.cpp
    gridcoin/quorum.cpp
    gridcoin/researcher.cpp
    gridcoin/scraper/http.cpp
    gridcoin/scraper/scraper.cpp
    gridcoin/scraper/scraper_net.cpp
    gridcoin/staking/difficulty.cpp
    gridcoin/staking/exceptions.cpp
    gridcoin/staking/kernel.cpp
    gridcoin/staking/reward.cpp
    gridcoin/staking/status.cpp
    gridcoin/superblock.cpp
    gridcoin/support/block_finder.cpp
    gridcoin/tally.cpp
    gridcoin/tx_message.cpp
    gridcoin/upgrade.cpp
    gridcoin/voting/builders.cpp
    gridcoin/voting/claims.cpp
    gridcoin/voting/poll.cpp
    gridcoin/voting/registry.cpp
    gridcoin/voting/result.cpp
    gridcoin/voting/vote.cpp
    hash.cpp
    init.cpp
    key.cpp
    key_io.cpp
    keystore.cpp
    logging.cpp
    main.cpp
    miner.cpp
    net.cpp
    netaddress.cpp
    netbase.cpp
    node/blockstorage.cpp
    node/ui_interface.cpp
    noui.cpp
    pbkdf2.cpp
    policy/policy.cpp
    primitives/transaction.cpp
    protocol.cpp
    pubkey.cpp
    random.cpp
    randomenv.cpp
    rpc/blockchain.cpp
    rpc/client.cpp
    rpc/dataacq.cpp
    rpc/mining.cpp
    rpc/misc.cpp
    rpc/net.cpp
    rpc/protocol.cpp
    rpc/rawtransaction.cpp
    rpc/server.cpp
    rpc/voting.cpp
    scheduler.cpp
    script.cpp
    scrypt-x86.S
    scrypt-x86_64.S
    scrypt.cpp
    support/cleanse.cpp
    support/lockedpool.cpp
    sync.cpp
    uint256.cpp
    util.cpp
    util/bip32.cpp
    util/settings.cpp
    util/strencodings.cpp
    util/string.cpp
    util/syserror.cpp
    util/system.cpp
    util/threadinterrupt.cpp
    util/threadnames.cpp
    util/time.cpp
    util/tokenpipe.cpp
    validation.cpp
    wallet/db.cpp
    wallet/diagnose.cpp
    wallet/rpcdump.cpp
    wallet/rpcwallet.cpp
    wallet/wallet.cpp
    wallet/walletdb.cpp
    wallet/walletutil.cpp
)

target_include_directories(gridcoin_util PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
)

target_link_libraries(gridcoin_util PUBLIC
    -lm ${ATOMICS_LIBRARIES} ${LIBUNIVALUE}
    BerkeleyDB::CXX
    Boost::filesystem Boost::headers Boost::iostreams Boost::thread
    CURL::libcurl
    OpenSSL::Crypto OpenSSL::SSL
    PkgConfig::SECP256K1
    Threads::Threads
    leveldb::leveldb
    libzip::zip
)
target_link_libraries(gridcoin_util PUBLIC ${LIBGRIDCOIN_CRYPTO})

target_compile_definitions(gridcoin_util PUBLIC
    HAVE_CONFIG_H
    HAVE_BUILD_INFO
)

if(ENABLE_UPNP)
    if(DEFAULT_UPNP)
        target_compile_definitions(gridcoin_util PRIVATE USE_UPNP=1)
    else()
        target_compile_definitions(gridcoin_util PRIVATE USE_UPNP=0)
    endif()

    if(WIN32)
        target_compile_definitions(gridcoin_util PRIVATE MINIUPNP_STATICLIB)
    endif()

    target_link_libraries(gridcoin_util PUBLIC PkgConfig::MINIUPNPC)
endif()


# Daemon
# ======

if(ENABLE_DAEMON)
    add_executable(gridcoinresearchd gridcoinresearchd.cpp)
    target_link_libraries(gridcoinresearchd PUBLIC gridcoin_util)

    if(WIN32)
        target_sources(gridcoinresearchd PRIVATE gridcoinresearchd-res.rc)
    endif()
endif()


# Qt GUI
# ======

if(ENABLE_GUI)
    add_subdirectory(qt)
endif()


# Tests
# =====

if(ENABLE_TESTS)
    add_subdirectory(test)
endif()
