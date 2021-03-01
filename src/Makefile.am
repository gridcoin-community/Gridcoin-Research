# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

DIST_SUBDIRS = univalue

AM_LDFLAGS = ${libcurl_LIBS} $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS)
AM_CXXFLAGS = $(HARDENED_CXXFLAGS) $(ERROR_CXXFLAGS)
AM_CPPFLAGS = ${libcurl_CFLAGS} $(HARDENED_CPPFLAGS) -DSTATICLIB -DCURL_STATICLIB -DMINIUPNP_STATICLIB -DZIP_STATIC -DNN_STATIC_LIB
EXTRA_LIBRARIES =

if EMBEDDED_UNIVALUE
LIBUNIVALUE = univalue/libunivalue.la

$(LIBUNIVALUE): $(wildcard univalue/lib/*) $(wildcard univalue/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)
else
LIBUNIVALUE = $(UNIVALUE_LIBS)
endif

GRIDCOIN_CONFIG_INCLUDES=-I$(builddir)/config
GRIDCOIN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BDB_CPPFLAGS) $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS) $(UNIVALUE_CFLAGS) $(CURL_CFLAGS) $(LIBZIP_CFLAGS)

LIBGRIDCOIN_UTIL=libgridcoin_util.a
LIBGRIDCOINQT=qt/libgridcoinqt.a
LIBGRIDCOIN_CRYPTO_BASE=crypto/libgridcoin_crypto_base.a

LIBGRIDCOIN_CRYPTO= $(LIBGRIDCOIN_CRYPTO_BASE)
if ENABLE_SSE41
LIBGRIDCOIN_CRYPTO_SSE41 = crypto/libgridcoin_crypto_sse41.a
LIBGRIDCOIN_CRYPTO += $(LIBGRIDCOIN_CRYPTO_SSE41)
endif
if ENABLE_AVX2
LIBGRIDCOIN_CRYPTO_AVX2 = crypto/libgridcoin_crypto_avx2.a
LIBGRIDCOIN_CRYPTO += $(LIBGRIDCOIN_CRYPTO_AVX2)
endif
if ENABLE_SHANI
LIBGRIDCOIN_CRYPTO_SHANI = crypto/libgridcoin_crypto_shani.a
LIBGRIDCOIN_CRYPTO += $(LIBGRIDCOIN_CRYPTO_SHANI)
endif

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES += $(LIBGRIDCOIN_UTIL) \
                   $(LIBGRIDCOIN_CRYPTO)


bin_PROGRAMS =
noinst_PROGRAMS =
TESTS =
BENCHMARKS =

if BUILD_GRIDCOINRESEARCHD
    bin_PROGRAMS += gridcoinresearchd
endif

.PHONY: FORCE
# gridcoin core #
GRIDCOIN_CORE_H = \
    addrdb.h \
    addrman.h \
    alert.h \
    amount.h \
    arith_uint256.h \
    attributes.h \
    banman.h \
    base58.h \
    bignum.h \
    chainparams.h \
    chainparamsbase.h \
    checkpoints.h \
    compat.h \
    compat/byteswap.h \
    compat/endian.h \
    consensus/consensus.h \
    consensus/params.h \
    crypter.h \
    fs.h \
    fwd.h \
    gridcoin/account.h \
    gridcoin/accrual/computer.h \
    gridcoin/accrual/newbie.h \
    gridcoin/accrual/null.h \
    gridcoin/accrual/research_age.h \
    gridcoin/accrual/snapshot.h \
    gridcoin/appcache.h \
    gridcoin/backup.h \
    gridcoin/beacon.h \
    gridcoin/block_index.h \
    gridcoin/boinc.h \
    gridcoin/claim.h \
    gridcoin/contract/contract.h \
    gridcoin/contract/handler.h \
    gridcoin/contract/message.h \
    gridcoin/contract/payload.h \
    gridcoin/cpid.h \
    gridcoin/gridcoin.h \
    gridcoin/magnitude.h \
    gridcoin/project.h \
    gridcoin/quorum.h \
    gridcoin/researcher.h \
    gridcoin/scraper/fwd.h \
    gridcoin/scraper/http.h \
    gridcoin/scraper/scraper.h \
    gridcoin/scraper/scraper_net.h \
    gridcoin/staking/chain_trust.h \
    gridcoin/staking/difficulty.h \
    gridcoin/staking/exceptions.h \
    gridcoin/staking/kernel.h \
    gridcoin/staking/reward.h \
    gridcoin/staking/spam.h \
    gridcoin/staking/status.h \
    gridcoin/superblock.h \
    gridcoin/support/block_finder.h \
    gridcoin/support/enumbytes.h \
    gridcoin/support/filehash.h \
    gridcoin/support/xml.h \
    gridcoin/tally.h \
    gridcoin/tx_message.h \
    gridcoin/upgrade.h \
    gridcoin/voting/builders.h \
    gridcoin/voting/claims.h \
    gridcoin/voting/fwd.h \
    gridcoin/voting/payloads.h \
    gridcoin/voting/poll.h \
    gridcoin/voting/registry.h \
    gridcoin/voting/result.h \
    gridcoin/voting/vote.h \
    hash.h \
    init.h \
    key.h \
    keystore.h \
    logging.h \
    main.h \
    miner.h \
    mruset.h \
    netbase.h \
    net.h \
    pbkdf2.h \
    prevector.h \
    protocol.h \
    reverselock.h \
    rpc/blockchain.h \
    rpc/client.h \
    rpc/protocol.h \
    rpc/server.h \
    scheduler.h \
    script.h \
    scrypt.h \
    serialize.h \
    span.h \
    streams.h \
    support/allocators/secure.h \
    support/allocators/zeroafterfree.h \
    support/cleanse.h \
    support/lockedpool.h \
    sync.h \
    threadsafety.h \
    tinyformat.h \
    txdb.h \
    txdb-leveldb.h \
    ui_interface.h \
    uint256.h \
    util/memory.h \
    util/reverse_iterator.h \
    util/strencodings.h \
    util/threadnames.h \
    util/time.h \
    util.h \
    version.h \
    wallet/coincontrol.h \
    wallet/db.h \
    wallet/walletdb.h \
    wallet/wallet.h \
    wallet/ismine.h

GRIDCOIN_CORE_CPP = addrdb.cpp \
    addrman.cpp \
    alert.cpp \
    arith_uint256.cpp \
    banman.cpp \
    chainparams.cpp \
    chainparamsbase.cpp \
    checkpoints.cpp \
    crypter.cpp \
    fs.cpp \
    gridcoin/appcache.cpp \
    gridcoin/backup.cpp \
    gridcoin/beacon.cpp \
    gridcoin/boinc.cpp \
    gridcoin/claim.cpp \
    gridcoin/contract/contract.cpp \
    gridcoin/contract/message.cpp \
    gridcoin/cpid.cpp \
    gridcoin/gridcoin.cpp \
    gridcoin/project.cpp \
    gridcoin/quorum.cpp \
    gridcoin/researcher.cpp \
    gridcoin/scraper/http.cpp \
    gridcoin/scraper/scraper.cpp \
    gridcoin/scraper/scraper_net.cpp \
    gridcoin/staking/difficulty.cpp \
    gridcoin/staking/exceptions.cpp \
    gridcoin/staking/kernel.cpp \
    gridcoin/staking/reward.cpp \
    gridcoin/staking/status.cpp \
    gridcoin/superblock.cpp \
    gridcoin/support/block_finder.cpp \
    gridcoin/tally.cpp \
    gridcoin/upgrade.cpp \
    gridcoin/voting/builders.cpp \
    gridcoin/voting/claims.cpp \
    gridcoin/voting/poll.cpp \
    gridcoin/voting/registry.cpp \
    gridcoin/voting/result.cpp \
    gridcoin/voting/vote.cpp \
    init.cpp \
    key.cpp \
    keystore.cpp \
    logging.cpp \
    main.cpp \
    miner.cpp \
    netbase.cpp \
    net.cpp \
    noui.cpp \
    pbkdf2.cpp \
    protocol.cpp \
    rpc/blockchain.cpp \
    rpc/client.cpp \
    rpc/dataacq.cpp \
    rpc/mining.cpp \
    rpc/misc.cpp \
    rpc/net.cpp \
    rpc/protocol.cpp \
    rpc/rawtransaction.cpp \
    rpc/server.cpp \
    rpc/voting.cpp \
    script.cpp \
    scrypt.cpp \
    scrypt-x86_64.S \
    scrypt-x86.S \
    scheduler.cpp \
    support/cleanse.cpp \
    support/lockedpool.cpp \
    sync.cpp \
    txdb-leveldb.cpp \
    uint256.cpp \
    util/strencodings.cpp \
    util/threadnames.cpp \
    util/time.cpp \
    util.cpp \
    version.cpp \
    wallet/db.cpp \
    wallet/rpcdump.cpp \
    wallet/rpcwallet.cpp \
    wallet/wallet.cpp \
    wallet/walletdb.cpp


obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh "$(abs_top_builddir)/src/obj/build.h" \
	  "$(abs_top_srcdir)"
libgridcoin_util_a-version.$(OBJEXT): obj/build.h

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libgridcoin_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(GRIDCOIN_INCLUDES)
libgridcoin_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libgridcoin_util_a_SOURCES = $(GRIDCOIN_CORE_CPP) \
    $(GRIDCOIN_CORE_H)

if TARGET_WINDOWS
libgridcoin_util_a_CXXFLAGS += -DUNICODE
endif

nodist_libgridcoin_util_a_SOURCES = $(srcdir)/obj/build.h


# crypto primitives library
crypto_libgridcoin_crypto_base_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libgridcoin_crypto_base_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libgridcoin_crypto_base_a_SOURCES = \
    crypto/aes.cpp \
    crypto/aes.h \
    crypto/chacha20.h \
    crypto/chacha20.cpp \
    crypto/common.h \
    crypto/hmac_sha256.cpp \
    crypto/hmac_sha256.h \
    crypto/hmac_sha512.cpp \
    crypto/hmac_sha512.h \
    crypto/ripemd160.cpp \
    crypto/ripemd160.h \
    crypto/sha1.cpp \
    crypto/sha1.h \
    crypto/sha256.cpp \
    crypto/sha256.h \
    crypto/sha512.cpp \
    crypto/sha512.h

if USE_ASM
crypto_libgridcoin_crypto_base_a_SOURCES += crypto/sha256_sse4.cpp
endif

crypto_libgridcoin_crypto_sse41_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libgridcoin_crypto_sse41_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libgridcoin_crypto_sse41_a_CXXFLAGS += $(SSE41_CXXFLAGS)
crypto_libgridcoin_crypto_sse41_a_CPPFLAGS += -DENABLE_SSE41
crypto_libgridcoin_crypto_sse41_a_SOURCES = crypto/sha256_sse41.cpp

crypto_libgridcoin_crypto_avx2_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libgridcoin_crypto_avx2_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libgridcoin_crypto_avx2_a_CXXFLAGS += $(AVX2_CXXFLAGS)
crypto_libgridcoin_crypto_avx2_a_CPPFLAGS += -DENABLE_AVX2
crypto_libgridcoin_crypto_avx2_a_SOURCES = crypto/sha256_avx2.cpp

crypto_libgridcoin_crypto_shani_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libgridcoin_crypto_shani_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libgridcoin_crypto_shani_a_CXXFLAGS += $(SHANI_CXXFLAGS)
crypto_libgridcoin_crypto_shani_a_CPPFLAGS += -DENABLE_SHANI
crypto_libgridcoin_crypto_shani_a_SOURCES = crypto/sha256_shani.cpp

CTAES_DIST =  crypto/ctaes/bench.c
CTAES_DIST += crypto/ctaes/ctaes.c
CTAES_DIST += crypto/ctaes/ctaes.h
CTAES_DIST += crypto/ctaes/README.md
CTAES_DIST += crypto/ctaes/test.c

# gridcoinresearchd binary #
gridcoinresearchd_SOURCES = gridcoinresearchd.cpp
gridcoinresearchd_CPPFLAGS = $(AM_CPPFLAGS) $(GRIDCOIN_INCLUDES)
gridcoinresearchd_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
gridcoinresearchd_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

gridcoinresearchd_LDADD = \
 $(LIBGRIDCOIN_UTIL) \
 $(LIBGRIDCOIN_CRYPTO) \
 $(LIBUNIVALUE) \
 $(LIBLEVELDB) \
 $(LIBLEVELDB_SSE42) \
 $(LIBMEMENV)

gridcoinresearchd_LDADD += $(CURL_LIBS) $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(LIBZIP_LIBS)

if TARGET_WINDOWS
gridcoinresearchd_SOURCES += gridcoinresearchd-res.rc
gridcoinresearchd_CPPFLAGS += -DUNICODE
endif

CLEANFILES = $(EXTRA_LIBRARIES)
CLEANFILES += univalue/*.gcda univalue/*.gcno
CLEANFILES += *.gcda *.gcno
CLEANFILES += compat/*.gcda compat/*.gcno
CLEANFILES += crypto/*.gcda crypto/*.gcno
CLEANFILES += obj/build.h

EXTRA_DIST = $(CTAES_DIST)


config/gridcoin-config.h: config/stamp-h1
	@$(MAKE) -C $(top_builddir) $(subdir)/$(@)
config/stamp-h1: $(top_srcdir)/$(subdir)/config/gridcoin-config.h.in $(top_builddir)/config.status
	$(AM_V_at)$(MAKE) -C $(top_builddir) $(subdir)/$(@)
$(top_srcdir)/$(subdir)/config/gridcoin-config.h.in:  $(am__configure_deps)
	$(AM_V_at)$(MAKE) -C $(top_srcdir) $(subdir)/config/gridcoin-config.h.in

clean-local:
	-$(MAKE) -C univalue clean
	-rm -f leveldb/*/*.gcda leveldb/*/*.gcno leveldb/helpers/memenv/*.gcda leveldb/helpers/memenv/*.gcno
	-rm -f config.h
	-rm -rf test/__pycache__

.rc.o:
	@test -f $(WINDRES)
	## FIXME: How to get the appropriate modulename_CPPFLAGS in here?
	$(AM_V_GEN) $(WINDRES) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) -DWINDRES_PREPROC -i $< -o $@

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(<D) $<

if EMBEDDED_LEVELDB
include Makefile.leveldb.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
