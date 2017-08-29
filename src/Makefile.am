# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

DIST_SUBDIRS = 

AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS)
AM_CXXFLAGS = $(HARDENED_CXXFLAGS) -fPIC $(ERROR_CXXFLAGS)
AM_CPPFLAGS = $(HARDENED_CPPFLAGS) -fPIC
EXTRA_LIBRARIES =

GRIDCOIN_CONFIG_INCLUDES=-I$(builddir)/config
GRIDCOIN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BDB_CPPFLAGS) $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS) $(CURL_CFLAGS) $(LIBZIP_CFLAGS)

LIBGRIDCOIN_UTIL=libgridcoin_util.a
LIBGRIDCOINQT=qt/libgridcoinqt.a

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES += $(LIBGRIDCOIN_UTIL) 

bin_PROGRAMS =
noinst_PROGRAMS =
TESTS =
BENCHMARKS =

if BUILD_GRIDCOINRESEARCHD
    bin_PROGRAMS += gridcoinresearchd
endif


.PHONY: FORCE check-symbols check-security
# gridcoin core #
GRIDCOIN_CORE_H = \
	addrman.h \
	alert.h \
	allocators.h \
	base58.h \
	beacon.h \
	bignum.h \
	bitcoinrpc.h \
	block.h \
	boinc.h \
	checkpoints.h \
	clientversion.h \
	coincontrol.h \
	compat.h \
	cpid.h \
	crypter.h \
	curlbuild.h \
	curl.h \
	curlrules.h \
	curlver.h \
	db.h \
	easy.h \
	fwd.h \
	gridcoin.h \
	grcrestarter.h \
	init.h \
	irc.h \
	kernel.h \
	key.h \
	keystore.h \
	main.h \
	miner.h \
	mprintf.h \
	mruset.h \
	multi.h \
	neuralnet.h \
	netbase.h \
	net.h \
	pbkdf2.h \
	protocol.h \
	script.h \
	scrypt.h \
	serialize.h \
	stdcheaders.h \
	strlcpy.h \
	sync.h \
	threadsafety.h \
	txdb.h \
	txdb-leveldb.h \
	ui_interface.h \
	uint256.h \
	upgrader.h \
	util.h \
	version.h \
	walletdb.h \
	wallet.h

GRIDCOIN_CORE_CPP = addrman.cpp \
	alert.cpp \
	allocators.cpp \
	beacon.cpp \
	bitcoinrpc.cpp \
	block.cpp \
	boinc.cpp \
	checkpoints.cpp \
	cpid.cpp \
	crypter.cpp \
	db.cpp \
	init.cpp \
	irc.cpp \
	kernel.cpp \
	key.cpp \
	keystore.cpp \
	main.cpp \
	miner.cpp \
	netbase.cpp \
	net.cpp \
	noui.cpp \
	pbkdf2.cpp \
	protocol.cpp \
	rpcblockchain.cpp \
	rpcdump.cpp \
	rpcmining.cpp \
	rpcnet.cpp \
	rpcrawtransaction.cpp \
	rpcwallet.cpp \
	script.cpp \
	scrypt.cpp \
	sync.cpp \
	txdb-leveldb.cpp \
	upgrader.cpp \
	util.cpp \
	version.cpp \
	wallet.cpp \
	walletdb.cpp

obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh "$(abs_top_builddir)/src/obj/build.h" \
	  "$(abs_top_srcdir)"
#libbitcoin_util_a-clientversion.$(OBJEXT): obj/build.h


# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libgridcoin_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(GRIDCOIN_INCLUDES)
libgridcoin_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libgridcoin_util_a_SOURCES = $(GRIDCOIN_CORE_CPP) \
    grcrestarter_stub.cpp \
    neuralnet_stub.cpp \
    $(GRIDCOIN_CORE_H)

if TARGET_WINDOWS
libgridcoin_util_a_CXXFLAGS += -DWIN32 -DUNICODE
endif

if GLIBC_BACK_COMPAT
libgridcoin_util_a_SOURCES += compat/glibc_compat.cpp
endif

# gridcoinresearchd binary #
gridcoinresearchd_SOURCES = gridcoinresearchd.cpp
gridcoinresearchd_CPPFLAGS = $(AM_CPPFLAGS) $(GRIDCOIN_INCLUDES)
gridcoinresearchd_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
gridcoinresearchd_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

gridcoinresearchd_LDADD = \
 $(LIBGRIDCOIN_UTIL) \
 $(LIBLEVELDB) \
 $(LIBLEVELDB_SSE42) \
 $(LIBMEMENV)

gridcoinresearchd_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(CURL_LIBS) $(LIBZIP_LIBS)

if TARGET_WINDOWS
gridcoinresearchd_SOURCES += gridcoinresearchd-res.rc
endif

CLEANFILES = $(EXTRA_LIBRARIES)

CLEANFILES += *.gcda *.gcno
CLEANFILES += compat/*.gcda compat/*.gcno
CLEANFILES += consensus/*.gcda consensus/*.gcno
CLEANFILES += crypto/*.gcda crypto/*.gcno
CLEANFILES += policy/*.gcda policy/*.gcno
CLEANFILES += primitives/*.gcda primitives/*.gcno
CLEANFILES += script/*.gcda script/*.gcno
CLEANFILES += support/*.gcda support/*.gcno
CLEANFILES += wallet/*.gcda wallet/*.gcno
CLEANFILES += wallet/test/*.gcda wallet/test/*.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST = $(CTAES_DIST)


config/gridcoin-config.h: config/stamp-h1
	@$(MAKE) -C $(top_builddir) $(subdir)/$(@)
config/stamp-h1: $(top_srcdir)/$(subdir)/config/gridcoin-config.h.in $(top_builddir)/config.status
	$(AM_V_at)$(MAKE) -C $(top_builddir) $(subdir)/$(@)
$(top_srcdir)/$(subdir)/config/gridcoin-config.h.in:  $(am__configure_deps)
	$(AM_V_at)$(MAKE) -C $(top_srcdir) $(subdir)/config/gridcoin-config.h.in

clean-local:
	-rm -f leveldb/*/*.gcda leveldb/*/*.gcno leveldb/helpers/memenv/*.gcda leveldb/helpers/memenv/*.gcno
	-rm -f config.h
	-rm -rf test/__pycache__

.rc.o:
	@test -f $(WINDRES)
	## FIXME: How to get the appropriate modulename_CPPFLAGS in here?
	$(AM_V_GEN) $(WINDRES) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) -DWINDRES_PREPROC -i $< -o $@

.mm.o:
	$(AM_V_CXX) $(OBJCXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(AM_CXXFLAGS) $(PIE_FLAGS) $(CXXFLAGS) -c -o $@ $<

check-symbols: $(bin_PROGRAMS)
if GLIBC_BACK_COMPAT
	@echo "Checking glibc back compat..."
	$(AM_V_at) READELF=$(READELF) CPPFILT=$(CPPFILT) $(top_srcdir)/contrib/devtools/symbol-check.py < $(bin_PROGRAMS)
endif

check-security: $(bin_PROGRAMS)
if HARDEN
	@echo "Checking binary security..."
	$(AM_V_at) READELF=$(READELF) OBJDUMP=$(OBJDUMP) $(top_srcdir)/contrib/devtools/security-check.py < $(bin_PROGRAMS)
endif

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
