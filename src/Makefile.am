# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.

# Pattern rule to print variables, e.g. make print-top_srcdir
print-%: FORCE
	@echo '$*'='$($*)'

DIST_SUBDIRS = univalue secp256k1

AM_LDFLAGS = ${libcurl_LIBS} $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS) $(SANITIZER_LDFLAGS)
AM_CXXFLAGS = $(HARDENED_CXXFLAGS) $(ERROR_CXXFLAGS) $(SANITIZER_CXXFLAGS)
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
GRIDCOIN_INCLUDES=-I$(builddir) -I$(builddir)/obj -I$(srcdir)/secp256k1/include $(BDB_CFLAGS) -I$(builddir)/bdb53 $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS) $(UNIVALUE_CFLAGS) $(CURL_CFLAGS) $(LIBZIP_CFLAGS)

LIBGRIDCOIN_UTIL=libgridcoin_util.a
LIBGRIDCOINQT=qt/libgridcoinqt.a
LIBGRIDCOIN_CRYPTO_BASE=crypto/libgridcoin_crypto_base.a
LIBSECP256K1=secp256k1/libsecp256k1.la
LIBDB=bdb53/libdb_cxx.a

LIBGRIDCOIN_CRYPTO= $(LIBGRIDCOIN_CRYPTO_BASE)
if ENABLE_SSE41
LIBGRIDCOIN_CRYPTO_SSE41 = crypto/libgridcoin_crypto_sse41.a
LIBGRIDCOIN_CRYPTO += $(LIBGRIDCOIN_CRYPTO_SSE41)
endif
if ENABLE_AVX2
LIBGRIDCOIN_CRYPTO_AVX2 = crypto/libgridcoin_crypto_avx2.a
LIBGRIDCOIN_CRYPTO += $(LIBGRIDCOIN_CRYPTO_AVX2)
endif
if ENABLE_X86_SHANI
LIBGRIDCOIN_CRYPTO_X86_SHANI = crypto/libgridcoin_crypto_x86_shani.a
LIBGRIDCOIN_CRYPTO += $(LIBGRIDCOIN_CRYPTO_X86_SHANI)
endif
if ENABLE_ARM_SHANI
LIBGRIDCOIN_CRYPTO_ARM_SHANI = crypto/libgridcoin_crypto_arm_shani.a
LIBGRIDCOIN_CRYPTO += $(LIBGRIDCOIN_CRYPTO_ARM_SHANI)
endif

$(LIBSECP256K1): $(wildcard secp256k1/src/*.h) $(wildcard secp256k1/src/*.c) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

$(LIBDB): $(wildcard bdb53/src/*/*.h) $(wildcard bdb53/src/*/*.c) $(wildcard bdb53/*.h)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

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
    chainparams.h \
    chainparamsbase.h \
    checkpoints.h \
    clientversion.h \
    compat.h \
    compat/assumptions.h \
    compat/byteswap.h \
    compat/cpuid.h \
    compat/endian.h \
    consensus/consensus.h \
    consensus/merkle.h \
    consensus/params.h \
    consensus/tx_verify.h \
    crypter.h \
    dbwrapper.h \
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
    gridcoin/contract/registry.h \
    gridcoin/contract/registry_db.h \
    gridcoin/cpid.h \
    gridcoin/gridcoin.h \
    gridcoin/magnitude.h \
    gridcoin/mrc.h \
    gridcoin/project.h \
    gridcoin/protocol.h \
    gridcoin/quorum.h \
    gridcoin/researcher.h \
    gridcoin/scraper/fwd.h \
    gridcoin/scraper/http.h \
    gridcoin/scraper/scraper.h \
    gridcoin/scraper/scraper_net.h \
    gridcoin/scraper/scraper_registry.h \
    gridcoin/sidestake.h \
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
    gridcoin/voting/filter.h \
    gridcoin/voting/fwd.h \
    gridcoin/voting/payloads.h \
    gridcoin/voting/poll.h \
    gridcoin/voting/registry.h \
    gridcoin/voting/result.h \
    gridcoin/voting/vote.h \
    hash.h \
    index/disktxpos.h \
    index/txindex.h \
    init.h \
    key.h \
    keystore.h \
    key_io.h \
    logging.h \
    main.h \
    miner.h \
    mruset.h \
    netbase.h \
    netaddress.h \
    net.h \
    node/blockstorage.h \
    pbkdf2.h \
    policy/fees.h \
    policy/policy.h \
    prevector.h \
    primitives/transaction.h \
    protocol.h \
    pubkey.h \
    random.h \
    randomenv.h \
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
    node/ui_interface.h \
    uint256.h \
    util/bip32.h \
    util/check.h \
    util/hash_type.h \
    util/macros.h \
    util/overflow.h \
    util/reverse_iterator.h \
    util/settings.h \
    util/strencodings.h \
    util/string.h \
    util/syserror.h \
    util/system.h \
    util/threadnames.h \
    util/threadinterrupt.h \
    util/time.h \
    util/tokenpipe.h \
    util.h \
    validation.h \
    version.h \
    wallet/coincontrol.h \
    wallet/db.h \
    wallet/diagnose.h \
    wallet/generated_type.h \
    wallet/ismine.h \
    wallet/wallet.h \
    wallet/walletdb.h \
    wallet/walletutil.h

GRIDCOIN_CORE_CPP = addrdb.cpp \
    addrman.cpp \
    alert.cpp \
    arith_uint256.cpp \
    banman.cpp \
    base58.cpp \
    chainparams.cpp \
    chainparamsbase.cpp \
    checkpoints.cpp \
    clientversion.cpp \
    consensus/merkle.cpp \
    consensus/tx_verify.cpp \
    crypter.cpp \
    dbwrapper.cpp \
    fs.cpp \
    gridcoin/backup.cpp \
    gridcoin/beacon.cpp \
    gridcoin/boinc.cpp \
    gridcoin/claim.cpp \
    gridcoin/contract/contract.cpp \
    gridcoin/contract/message.cpp \
    gridcoin/contract/registry.cpp \
    gridcoin/cpid.cpp \
    gridcoin/gridcoin.cpp \
    gridcoin/mrc.cpp \
    gridcoin/project.cpp \
    gridcoin/protocol.cpp \
    gridcoin/quorum.cpp \
    gridcoin/researcher.cpp \
    gridcoin/scraper/http.cpp \
    gridcoin/scraper/scraper.cpp \
    gridcoin/scraper/scraper_net.cpp \
    gridcoin/scraper/scraper_registry.cpp \
    gridcoin/sidestake.cpp \
    gridcoin/staking/difficulty.cpp \
    gridcoin/staking/exceptions.cpp \
    gridcoin/staking/kernel.cpp \
    gridcoin/staking/reward.cpp \
    gridcoin/staking/status.cpp \
    gridcoin/superblock.cpp \
    gridcoin/support/block_finder.cpp \
    gridcoin/tally.cpp \
    gridcoin/tx_message.cpp \
    gridcoin/upgrade.cpp \
    gridcoin/voting/builders.cpp \
    gridcoin/voting/claims.cpp \
    gridcoin/voting/poll.cpp \
    gridcoin/voting/registry.cpp \
    gridcoin/voting/result.cpp \
    gridcoin/voting/vote.cpp \
    hash.cpp \
    init.cpp \
    key.cpp \
    keystore.cpp \
    key_io.cpp \
    logging.cpp \
    main.cpp \
    miner.cpp \
    netbase.cpp \
    netaddress.cpp \
    net.cpp \
    node/blockstorage.cpp \
    node/ui_interface.cpp \
    noui.cpp \
    pbkdf2.cpp \
    policy/policy.cpp \
    primitives/transaction.cpp \
    protocol.cpp \
    pubkey.cpp \
    random.cpp \
    randomenv.cpp \
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
    uint256.cpp \
    util/bip32.cpp \
    util/settings.cpp \
    util/strencodings.cpp \
    util/string.cpp \
    util/syserror.cpp \
    util/system.cpp \
    util/threadinterrupt.cpp \
    util/threadnames.cpp \
    util/time.cpp \
    util/tokenpipe.cpp \
    util.cpp \
    validation.cpp \
    wallet/db.cpp \
    wallet/diagnose.cpp \
    wallet/rpcdump.cpp \
    wallet/rpcwallet.cpp \
    wallet/wallet.cpp \
    wallet/walletdb.cpp \
    wallet/walletutil.cpp


obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh "$(abs_top_builddir)/src/obj/build.h" \
	  "$(abs_top_srcdir)"
libgridcoin_util_a-clientversion.$(OBJEXT): obj/build.h

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# sanity checks are linked.
libgridcoin_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(GRIDCOIN_INCLUDES)
libgridcoin_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libgridcoin_util_a_SOURCES = $(GRIDCOIN_CORE_CPP) \
    $(GRIDCOIN_CORE_H)

if TARGET_WINDOWS
libgridcoin_util_a_CXXFLAGS += -DUNICODE
endif

nodist_libgridcoin_util_a_SOURCES = $(srcdir)/obj/build.h


# crypto primitives library
crypto_libgridcoin_crypto_base_a_CFLAGS = $(AM_CFLAGS) $(PIE_FLAGS)
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
    crypto/sha3.cpp \
    crypto/sha3.h \
    crypto/hmac_sha512.cpp \
    crypto/hmac_sha512.h \
    crypto/poly1305.h \
    crypto/poly1305.cpp \
    crypto/ripemd160.cpp \
    crypto/ripemd160.h \
    crypto/sha1.cpp \
    crypto/sha1.h \
    crypto/sha256.cpp \
    crypto/sha256.h \
    crypto/sha512.cpp \
    crypto/sha512.h \
    crypto/siphash.cpp \
    crypto/siphash.h \
    gridcoin/md5.c \
    gridcoin/md5.h

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

crypto_libgridcoin_crypto_x86_shani_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libgridcoin_crypto_x86_shani_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libgridcoin_crypto_x86_shani_a_CXXFLAGS += $(X86_SHANI_CXXFLAGS)
crypto_libgridcoin_crypto_x86_shani_a_CPPFLAGS += -DENABLE_X86_SHANI
crypto_libgridcoin_crypto_x86_shani_a_SOURCES = crypto/sha256_x86_shani.cpp

crypto_libgridcoin_crypto_arm_shani_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libgridcoin_crypto_arm_shani_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libgridcoin_crypto_arm_shani_a_CXXFLAGS += $(ARM_SHANI_CXXFLAGS)
crypto_libgridcoin_crypto_arm_shani_a_CPPFLAGS += -DENABLE_ARM_SHANI
crypto_libgridcoin_crypto_arm_shani_a_SOURCES = crypto/sha256_arm_shani.cpp

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
 $(LIBMEMENV) \
 $(LIBSECP256K1)

if ENABLE_WALLET
if EMBEDDED_BDB
gridcoinresearchd_LDADD += $(LIBDB)
else
gridcoinresearchd_LDADD += $(BDB_LIBS)
endif
endif

gridcoinresearchd_LDADD += $(CURL_LIBS) $(BOOST_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(LIBZIP_LIBS)

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
	-$(MAKE) -C secp256k1 clean
	-$(MAKE) -C univalue clean
	-$(MAKE) -C bdb53 clean
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

include Makefile.crc32c.include
include Makefile.leveldb.include

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
