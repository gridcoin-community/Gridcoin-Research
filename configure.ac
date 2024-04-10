dnl require autoconf 2.60 (AS_ECHO/AS_ECHO_N)
AC_PREREQ([2.60])
define(_CLIENT_VERSION_MAJOR, 5)
define(_CLIENT_VERSION_MINOR, 4)
define(_CLIENT_VERSION_REVISION, 8)
define(_CLIENT_VERSION_BUILD, 0)
define(_CLIENT_VERSION_IS_RELEASE, true)
define(_COPYRIGHT_YEAR, 2024)
define(_COPYRIGHT_HOLDERS,[The %s developers])
define(_COPYRIGHT_HOLDERS_SUBSTITUTION,[[Gridcoin]])
AC_INIT([Gridcoin],[_CLIENT_VERSION_MAJOR._CLIENT_VERSION_MINOR._CLIENT_VERSION_REVISION],[https://github.com/gridcoin/Gridcoin-Research/issues],[gridcoin],[https://gridcoin.us/])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([src/config/gridcoin-config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux/m4])

m4_ifndef([PKG_PROG_PKG_CONFIG], [m4_fatal([PKG_PROG_PKG_CONFIG macro not found. Please install pkg-config and re-run autogen.sh])])
PKG_PROG_PKG_CONFIG
if test "$PKG_CONFIG" = ""; then
  AC_MSG_ERROR([pkg-config not found])
fi

GRIDCOIN_DAEMON_NAME=gridcoinresearchd
GRIDCOIN_GUI_NAME=gridcoinresearch

AC_CANONICAL_HOST

AH_TOP([#ifndef GRIDCOIN_CONFIG_H])
AH_TOP([#define GRIDCOIN_CONFIG_H])
AH_BOTTOM([#endif //GRIDCOIN_CONFIG_H])

dnl faketime breaks configure and is only needed for make. Disable it here.
unset FAKETIME

dnl Automake init set-up and checks
AM_INIT_AUTOMAKE([no-define subdir-objects foreign])

dnl faketime messes with timestamps and causes configure to be re-run.
dnl --disable-maintainer-mode can be used to bypass this.
AM_MAINTAINER_MODE([enable])

dnl make the compilation flags quiet unless V=1 is used
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Compiler checks (here before libtool).
if test "${CXXFLAGS+set}" = "set"; then
  CXXFLAGS_overridden=yes
else
  CXXFLAGS_overridden=no
fi
AC_PROG_CXX
AM_PROG_AS

dnl By default, libtool for mingw refuses to link static libs into a dll for
dnl fear of mixing pic/non-pic objects, and import/export complications. Since
dnl we have those under control, re-enable that functionality.
case $host in
  *mingw*)
     lt_cv_deplibs_check_method="pass_all"
  ;;
esac

dnl Require C++17 compiler (no GNU extensions)
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory], [nodefault])
dnl Check if -latomic is required for <std::atomic>
CHECK_ATOMIC

dnl Unless the user specified OBJCXX, force it to be the same as CXX. This ensures
dnl that we get the same -std flags for both.
m4_ifdef([AC_PROG_OBJCXX],[
if test "${OBJCXX+set}" = ""; then
  OBJCXX="${CXX}"
fi
AC_PROG_OBJCXX
])

dnl Libtool init checks.
LT_INIT([pic-only win32-dll])

dnl Check/return PATH for base programs.
AC_PATH_TOOL(AR, ar)
AC_PATH_TOOL(RANLIB, ranlib)
AC_PATH_TOOL(STRIP, strip)
AC_PATH_TOOL(GCOV, gcov)
AC_PATH_PROG(LCOV, lcov)
dnl Python 3.6 is specified in .python-version and should be used if available, see doc/dependencies.md
AC_PATH_PROGS([PYTHON], [python3.6 python3.7 python3.8 python3.9 python3.10 python3.11 python3 python])
AC_PATH_PROG(GENHTML, genhtml)
AC_PATH_PROG([GIT], [git])
AC_PATH_PROG(CCACHE,ccache)
AC_PATH_PROG(XGETTEXT,xgettext)
AC_PATH_PROG(HEXDUMP,hexdump)
AC_PATH_TOOL(READELF, readelf)
AC_PATH_TOOL(CPPFILT, c++filt)
AC_PATH_TOOL(OBJCOPY, objcopy)
AC_PATH_PROG(DOXYGEN, doxygen)
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])

AC_ARG_VAR(PYTHONPATH, Augments the default search path for python module files)

# Enable wallet
AC_ARG_ENABLE([wallet],
  [AS_HELP_STRING([--disable-wallet],
  [disable wallet (enabled by default)])],
  [enable_wallet=$enableval],
  [enable_wallet=yes])

AC_ARG_WITH([miniupnpc],
  [AS_HELP_STRING([--with-miniupnpc],
  [enable UPNP (default is yes if libminiupnpc is found)])],
  [use_upnp=$withval],
  [use_upnp=auto])

AC_ARG_ENABLE([upnp-default],
  [AS_HELP_STRING([--enable-upnp-default],
  [if UPNP is enabled, turn it on at startup (default is no)])],
  [use_upnp_default=$enableval],
  [use_upnp_default=no])

AC_ARG_ENABLE(tests,
    AS_HELP_STRING([--disable-tests],[do not compile tests (default is to compile)]),
    [use_tests=$enableval],
    [use_tests=yes])

AC_ARG_ENABLE(gui-tests,
    AS_HELP_STRING([--disable-gui-tests],[do not compile GUI tests (default is to compile if GUI and tests enabled)]),
    [use_gui_tests=$enableval],
    [use_gui_tests=$use_tests])

AC_ARG_ENABLE(bench,
    AS_HELP_STRING([--disable-bench],[do not compile benchmarks (default is to compile)]),
    [use_bench=$enableval],
    [use_bench=yes])

AC_ARG_ENABLE([extended-functional-tests],
    AS_HELP_STRING([--enable-extended-functional-tests],[enable expensive functional tests when using lcov (default no)]),
    [use_extended_functional_tests=$enableval],
    [use_extended_functional_tests=no])

AC_ARG_WITH([qrencode],
  [AS_HELP_STRING([--with-qrencode],
  [enable QR code support (default is yes if qt is enabled and libqrencode is found)])],
  [use_qr=$withval],
  [use_qr=yes])

AC_ARG_ENABLE([hardening],
  [AS_HELP_STRING([--disable-hardening],
  [do not attempt to harden the resulting executables (default is to harden)])],
  [use_hardening=$enableval],
  [use_hardening=yes])

AC_ARG_ENABLE([reduce-exports],
  [AS_HELP_STRING([--enable-reduce-exports],
  [attempt to reduce exported symbols in the resulting executables (default is no)])],
  [use_reduce_exports=$enableval],
  [use_reduce_exports=no])

AC_ARG_ENABLE([ccache],
  [AS_HELP_STRING([--disable-ccache],
  [do not use ccache for building (default is to use if found)])],
  [use_ccache=$enableval],
  [use_ccache=auto])

AC_ARG_ENABLE([lcov],
  [AS_HELP_STRING([--enable-lcov],
  [enable lcov testing (default is no)])],
  [use_lcov=yes],
  [use_lcov=no])

AC_ARG_ENABLE([lcov-branch-coverage],
  [AS_HELP_STRING([--enable-lcov-branch-coverage],
  [enable lcov testing branch coverage (default is no)])],
  [use_lcov_branch=yes],
  [use_lcov_branch=no])

AC_ARG_ENABLE([threadlocal],
  [AS_HELP_STRING([--enable-threadlocal],
  [enable features that depend on the c++ thread_local keyword (currently just thread names in debug logs). (default is to enable if there is platform support)])],
  [use_thread_local=$enableval],
  [use_thread_local=auto])

AC_ARG_ENABLE([asm],
  [AS_HELP_STRING([--disable-asm],
  [disable assembly routines (enabled by default)])],
  [use_asm=$enableval],
  [use_asm=yes])

AC_ARG_ENABLE([embedded-bdb],
  [AS_HELP_STRING([--disable-embedded-bdb],
  [use embedded bdb instead of the system one (enabled by default)])],
  [enable_embedded_bdb=$enableval],
  [enable_embedded_bdb=yes])

if test "$use_asm" = "yes"; then
  AC_DEFINE(USE_ASM, 1, [Define this symbol to build in assembly routines])
fi

BUILD_TARGET=`$CC -dumpmachine 2>&1`
case $BUILD_TARGET in
  x86_64-*-*|amd64-*-*)
    have_x86_64=true
    ;;
esac

# Enable debug
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
                    [use debug compiler flags and macros (default is no)])],
    [enable_debug=$enableval],
    [enable_debug=no])

dnl Enable different -fsanitize options
AC_ARG_WITH([sanitizers],
    [AS_HELP_STRING([--with-sanitizers],
                    [comma separated list of extra sanitizers to build with (default is none enabled)])],
    [use_sanitizers=$withval])

# Turn warnings into errors
AC_ARG_ENABLE([werror],
    [AS_HELP_STRING([--enable-werror],
                    [Treat certain compiler warnings as errors (default is no)])],
    [enable_werror=$enableval],
    [enable_werror=no])

AC_LANG_PUSH([C++])
AX_CHECK_COMPILE_FLAG([-Werror],[CXXFLAG_WERROR="-Werror"],[CXXFLAG_WERROR=""])

if test "$enable_debug" = "yes"; then
    CPPFLAGS="$CPPFLAGS -DDEBUG -DDEBUG_LOCKORDER"
    if test "$GCC" = "yes"; then
        CFLAGS="$CFLAGS -g3 -O0"
    fi

    if test "$GXX" = "yes"; then
        CXXFLAGS="$CXXFLAGS -g3 -O0"
    fi
fi

if test "$use_sanitizers" != ""; then
  dnl First check if the compiler accepts flags. If an incompatible pair like
  dnl -fsanitize=address,thread is used here, this check will fail. This will also
  dnl fail if a bad argument is passed, e.g. -fsanitize=undfeined
  AX_CHECK_COMPILE_FLAG(
    [-fsanitize=$use_sanitizers],
    [SANITIZER_CXXFLAGS="-fsanitize=$use_sanitizers"],
    [AC_MSG_ERROR([compiler did not accept requested flags])])

  dnl Some compilers (e.g. GCC) require additional libraries like libasan,
  dnl libtsan, libubsan, etc. Make sure linking still works with the sanitize
  dnl flag. This is a separate check so we can give a better error message when
  dnl the sanitize flags are supported by the compiler but the actual sanitizer
  dnl libs are missing.
  AX_CHECK_LINK_FLAG(
    [-fsanitize=$use_sanitizers],
    [SANITIZER_LDFLAGS="-fsanitize=$use_sanitizers"],
    [AC_MSG_ERROR([linker did not accept requested flags, you are missing required libraries])],
    [],
    [AC_LANG_PROGRAM([[
    #include <cstdint>
    #include <cstddef>
    extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) { return 0; }
    __attribute__((weak)) // allow for libFuzzer linking
    ]],[[]])])
fi

ERROR_CXXFLAGS=
if test "$enable_werror" = "yes"; then
  if test "$CXXFLAG_WERROR" = ""; then
    AC_MSG_ERROR("enable-werror set but -Werror is not usable")
  fi
  AX_CHECK_COMPILE_FLAG([-Werror=vla],[ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror=vla"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Werror=shadow-field],[ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror=shadow-field"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Werror=switch],[ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror=switch"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Werror=thread-safety],[ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror=thread-safety"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Werror=range-loop-analysis],[ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror=range-loop-analysis"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Werror=unused-variable],[ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror=unused-variable"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Werror=date-time],[ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror=date-time"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Werror=return-type],[ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror=return-type"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Werror=conditional-uninitialized],[ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror=conditional-uninitialized"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Werror=unreachable-code-loop-increment],[ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror=unreachable-code-loop-increment"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Werror=mismatched-tags], [ERROR_CXXFLAGS="$ERROR_CXXFLAGS -Werror=mismatched-tags"], [], [$CXXFLAG_WERROR])
fi

if test "$CXXFLAGS_overridden" = "no"; then
  AX_CHECK_COMPILE_FLAG([-Wall],[CXXFLAGS="$CXXFLAGS -Wall"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wextra],[CXXFLAGS="$CXXFLAGS -Wextra"],,[[$CXXFLAG_WERROR]])
  dnl some compilers will ignore -Wformat-security without -Wformat, so just combine the two here.
  AX_CHECK_COMPILE_FLAG([-Wformat -Wformat-security],[CXXFLAGS="$CXXFLAGS -Wformat -Wformat-security"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wvla],[CXXFLAGS="$CXXFLAGS -Wvla"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wshadow-field],[CXXFLAGS="$CXXFLAGS -Wshadow-field"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wthread-safety],[CXXFLAGS="$CXXFLAGS -Wthread-safety"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wloop-analysis],[CXXFLAGS="$CXXFLAGS -Wloop-analysis"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wunused-member-function],[CXXFLAGS="$CXXFLAGS -Wunused-member-function"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wdate-time],[CXXFLAGS="$CXXFLAGS -Wdate-time"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wconditional-uninitialized],[CXXFLAGS="$CXXFLAGS -Wconditional-uninitialized"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wduplicated-branches],[CXXFLAGS="$CXXFLAGS -Wduplicated-branches"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wduplicated-cond],[CXXFLAGS="$CXXFLAGS -Wduplicated-cond"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wlogical-op],[CXXFLAGS="$CXXFLAGS -Wlogical-op"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Woverloaded-virtual],[CXXFLAGS="$CXXFLAGS -Woverloaded-virtual"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wunreachable-code-loop-increment],[CXXFLAGS="$CXXFLAGS -Wunreachable-code-loop-increment"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wimplicit-fallthrough], [CXXFLAGS="$CXXFLAGS -Wimplicit-fallthrough"], [], [$CXXFLAG_WERROR])

  ## Some compilers (gcc) ignore unknown -Wno-* options, but warn about all
  ## unknown options if any other warning is produced. Test the -Wfoo case, and
  ## set the -Wno-foo case if it works.
  AX_CHECK_COMPILE_FLAG([-Wunused-parameter],[CXXFLAGS="$CXXFLAGS -Wno-unused-parameter"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wself-assign],[CXXFLAGS="$CXXFLAGS -Wno-self-assign"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wdeprecated-register],[CXXFLAGS="$CXXFLAGS -Wno-deprecated-register"],,[[$CXXFLAG_WERROR]])
  AX_CHECK_COMPILE_FLAG([-Wimplicit-fallthrough],[CXXFLAGS="$CXXFLAGS -Wno-implicit-fallthrough"],,[[$CXXFLAG_WERROR]])
  dnl For Qt versions < 5.14 with GCC 9+:
  AX_CHECK_COMPILE_FLAG([-Wdeprecated-copy],[CXXFLAGS="$CXXFLAGS -Wno-deprecated-copy"],,[[$CXXFLAG_WERROR]])
fi


dnl x87 FP operations on x86 hosts can break assumptions made about the floating point values.
dnl See the commit message which introduced this change for more details.
case $host in
 i?86-*|x86_64-*)
    AX_CHECK_COMPILE_FLAG([-msse2],[CXXFLAGS="$CXXFLAGS -msse2 -mfpmath=sse"],[AC_MSG_ERROR([SSE2 support is required on x86 targets.])],[[$CXXFLAG_WERROR]])
    ;;
 *) ;;
esac

enable_sse42=no
enable_sse41=no
enable_avx2=no
enable_x86_shani=no

if test "$use_asm" = "yes"; then

dnl Check for optional instruction set support. Enabling these does _not_ imply that all code will
dnl be compiled with them, rather that specific objects/libs may use them after checking for runtime
dnl compatibility.

dnl x86
AX_CHECK_COMPILE_FLAG([-msse4.2],[[SSE42_CXXFLAGS="-msse4.2"]],,[[$CXXFLAG_WERROR]])
AX_CHECK_COMPILE_FLAG([-msse4.1],[[SSE41_CXXFLAGS="-msse4.1"]],,[[$CXXFLAG_WERROR]])
AX_CHECK_COMPILE_FLAG([-mavx -mavx2],[[AVX2_CXXFLAGS="-mavx -mavx2"]],,[[$CXXFLAG_WERROR]])
AX_CHECK_COMPILE_FLAG([-msse4 -msha], [X86_SHANI_CXXFLAGS="-msse4 -msha"], [], [$CXXFLAG_WERROR])

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $SSE42_CXXFLAGS"
AC_MSG_CHECKING(for SSE4.2 intrinsics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #if defined(_MSC_VER)
    #include <intrin.h>
    #elif defined(__GNUC__) && defined(__SSE4_2__)
    #include <nmmintrin.h>
    #endif
  ]],[[
    uint64_t l = 0;
    l = _mm_crc32_u8(l, 0);
    l = _mm_crc32_u32(l, 0);
    l = _mm_crc32_u64(l, 0);
    return l;
  ]])],
 [ AC_MSG_RESULT(yes); enable_sse42=yes],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $SSE41_CXXFLAGS"
AC_MSG_CHECKING(for SSE4.1 intrinsics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #include <immintrin.h>
  ]],[[
    __m128i l = _mm_set1_epi32(0);
    return _mm_extract_epi32(l, 3);
  ]])],
 [ AC_MSG_RESULT(yes); enable_sse41=yes; AC_DEFINE(ENABLE_SSE41, 1, [Define this symbol to build code that uses SSE4.1 intrinsics]) ],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $AVX2_CXXFLAGS"
AC_MSG_CHECKING(for AVX2 intrinsics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #include <immintrin.h>
  ]],[[
    __m256i l = _mm256_set1_epi32(0);
    return _mm256_extract_epi32(l, 7);
  ]])],
 [ AC_MSG_RESULT(yes); enable_avx2=yes; AC_DEFINE(ENABLE_AVX2, 1, [Define this symbol to build code that uses AVX2 intrinsics]) ],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $X86_SHANI_CXXFLAGS"
AC_MSG_CHECKING([for x86 SHA-NI intrinsics])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #include <immintrin.h>
  ]],[[
    __m128i i = _mm_set1_epi32(0);
    __m128i j = _mm_set1_epi32(1);
    __m128i k = _mm_set1_epi32(2);
    return _mm_extract_epi32(_mm_sha256rnds2_epu32(i, i, k), 0);
  ]])],
 [ AC_MSG_RESULT([yes]); enable_x86_shani=yes; AC_DEFINE([ENABLE_X86_SHANI], [1], [Define this symbol to build code that uses x86 SHA-NI intrinsics]) ],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

# ARM
AX_CHECK_COMPILE_FLAG([-march=armv8-a+crc+crypto],[[ARM_CRC_CXXFLAGS="-march=armv8-a+crc+crypto"]],,[[$CXXFLAG_WERROR]])
AX_CHECK_COMPILE_FLAG([-march=armv8-a+crc+crypto], [ARM_SHANI_CXXFLAGS="-march=armv8-a+crc+crypto"], [], [$CXXFLAG_WERROR])

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $ARM_CRC_CXXFLAGS"
AC_MSG_CHECKING([for ARMv8 CRC32 intrinsics])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <arm_acle.h>
    #include <arm_neon.h>
  ]],[[
#ifdef __aarch64__
    __crc32cb(0, 0); __crc32ch(0, 0); __crc32cw(0, 0); __crc32cd(0, 0);
    vmull_p64(0, 0);
#else
#error "crc32c library does not support hardware acceleration on 32-bit ARM"
#endif
  ]])],
 [ AC_MSG_RESULT(yes); enable_arm_crc=yes; ],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $ARM_SHANI_CXXFLAGS"
AC_MSG_CHECKING([for ARMv8 SHA-NI intrinsics])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <arm_acle.h>
    #include <arm_neon.h>
  ]],[[
    uint32x4_t a, b, c;
    vsha256h2q_u32(a, b, c);
    vsha256hq_u32(a, b, c);
    vsha256su0q_u32(a, b);
    vsha256su1q_u32(a, b, c);
  ]])],
 [ AC_MSG_RESULT([yes]); enable_arm_shani=yes; AC_DEFINE([ENABLE_ARM_SHANI], [1], [Define this symbol to build code that uses ARMv8 SHA-NI intrinsics]) ],
 [ AC_MSG_RESULT([no])]
)
CXXFLAGS="$TEMP_CXXFLAGS"

fi

CPPFLAGS="$CPPFLAGS -DBOOST_SPIRIT_THREADSAFE -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS"

AC_ARG_WITH([utils],
  [AS_HELP_STRING([--with-utils],
  [build bitcoin-cli bitcoin-tx (default=yes)])],
  [build_bitcoin_utils=$withval],
  [build_bitcoin_utils=yes])

AC_ARG_WITH([libs],
  [AS_HELP_STRING([--with-libs],
  [build libraries (default=yes)])],
  [build_bitcoin_libs=$withval],
  [build_bitcoin_libs=yes])

AC_ARG_WITH([daemon],
  [AS_HELP_STRING([--with-daemon],
  [build gridcoinresearchd daemon (default=yes)])],
  [build_gridcoinresearchd=$withval],
  [build_gridcoinresearchd=yes])

case $host in
  *mingw*)
     TARGET_OS=windows
     AC_CHECK_LIB([mingwthrd],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([kernel32],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([user32],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([gdi32],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([comdlg32],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([winspool],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([winmm],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([shell32],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([comctl32],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([ole32],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([oleaut32],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([uuid],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([rpcrt4],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([advapi32],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([ws2_32],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([mswsock],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([shlwapi],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([iphlpapi],      [main],, AC_MSG_ERROR(lib missing))
     AC_CHECK_LIB([crypt32],      [main],, AC_MSG_ERROR(lib missing))

     # -static is interpreted by libtool, where it has a different meaning.
     # In libtool-speak, it's -all-static.
     AX_CHECK_LINK_FLAG([[-static]],[LIBTOOL_APP_LDFLAGS="$LIBTOOL_APP_LDFLAGS -all-static"])

     AC_PATH_PROG([MAKENSIS], [makensis], none)
     if test "$MAKENSIS" = "none"; then
       AC_MSG_WARN("makensis not found. Cannot create installer.")
     fi

     AC_PATH_TOOL(WINDRES, windres, none)
     if test "$WINDRES" = "none"; then
       AC_MSG_ERROR("windres not found")
     fi

     CPPFLAGS="$CPPFLAGS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB -D_WIN32_WINNT=0x0601"
     case $host in
       i?86-*) WINDOWS_BITS=32 ;;
       x86_64-*) WINDOWS_BITS=64 ;;
       *) AC_MSG_ERROR("Could not determine win32/win64 for installer") ;;
     esac
     AC_SUBST(WINDOWS_BITS)

     dnl libtool insists upon adding -nostdlib and a list of objects/libs to link against.
     dnl That breaks our ability to build dll's with static libgcc/libstdc++/libssp. Override
     dnl its command here, with the predeps/postdeps removed, and -static inserted. Postdeps are
     dnl also overridden to prevent their insertion later.
     dnl This should only affect dll's.
     archive_cmds_CXX="\$CC -shared \$libobjs \$deplibs \$compiler_flags -static -o \$output_objdir/\$soname \${wl}--enable-auto-image-base -Xlinker --out-implib -Xlinker \$lib"
     postdeps_CXX=

     dnl We require Windows 7 (NT 6.1) or later
     AX_CHECK_LINK_FLAG([[-Wl,--major-subsystem-version -Wl,6 -Wl,--minor-subsystem-version -Wl,1]],[LDFLAGS="$LDFLAGS -Wl,--major-subsystem-version -Wl,6 -Wl,--minor-subsystem-version -Wl,1"],,[[$LDFLAG_WERROR]])
     ;;
  *darwin*)
     TARGET_OS=darwin
     if  test "$cross_compiling" != "yes"; then
       BUILD_OS=darwin

       AC_PATH_PROGS([RSVG_CONVERT], [rsvg-convert rsvg],rsvg-convert)
       AC_CHECK_PROG([BREW],brew, brew)
       if test "$BREW" = "brew"; then
         dnl These Homebrew packages may be keg-only, meaning that they won't be found
         dnl in expected paths because they may conflict with system files. Ask
         dnl Homebrew where each one is located, then adjust paths accordingly.
         dnl It's safe to add these paths even if the functionality is disabled by
         dnl the user (--without-wallet or --without-gui for example).
         openssl_prefix=`$BREW --prefix openssl@1.1 2>/dev/null`
         qt5_prefix=`$BREW --prefix qt@5 2>/dev/null`
         if test "$openssl_prefix" != ""; then
           PKG_CONFIG_PATH="$openssl_prefix/lib/pkgconfig:$PKG_CONFIG_PATH"
           export PKG_CONFIG_PATH
         fi

         if test "$qt5_prefix" != ""; then
           PKG_CONFIG_PATH="$qt5_prefix/lib/pkgconfig:$PKG_CONFIG_PATH"
           export PKG_CONFIG_PATH
         fi
       fi
     else
       case $build_os in
         *darwin*)
           BUILD_OS=darwin
           ;;
         *)
           AC_PATH_TOOL([DSYMUTIL], [dsymutil], dsymutil)
           AC_PATH_TOOL([INSTALLNAMETOOL], [install_name_tool], install_name_tool)
           AC_PATH_TOOL([OTOOL], [otool], otool)
           AC_PATH_PROGS([XORRISOFS], [xorrisofs], xorrisofs)
           AC_PATH_PROGS([DMG], [dmg], dmg)
           AC_PATH_PROGS([RSVG_CONVERT], [rsvg-convert rsvg],rsvg-convert)
           AC_PATH_PROGS([IMAGEMAGICK_CONVERT], [convert],convert)
           AC_PATH_PROGS([TIFFCP], [tiffcp],tiffcp)

           dnl libtool will try to strip the static lib, which is a problem for
           dnl cross-builds because strip attempts to call a hard-coded ld,
           dnl which may not exist in the path. Stripping the .a is not
           dnl necessary, so just disable it.
           old_striplib=
           ;;
       esac
     fi

     AX_CHECK_LINK_FLAG([[-Wl,-headerpad_max_install_names]], [LDFLAGS="$LDFLAGS -Wl,-headerpad_max_install_names"])
     CPPFLAGS="$CPPFLAGS -DMAC_OSX"
     OBJCXXFLAGS="$CXXFLAGS"
     ;;
   *linux*)
     TARGET_OS=linux
     LIBS="$LIBS -lrt"
     ;;
   *freebsd*)
     ;;
   *openbsd*)
     ;;
   *)
     OTHER_OS=`echo ${host_os} | awk '{print toupper($0)}'`
     AC_MSG_WARN([Guessing LevelDB OS as OS_${OTHER_OS}, please check whether this is correct, if not add an entry to configure.ac.])
     ;;
esac

dnl Suppress ABI compatibility warnings for ARM:
dnl
dnl   "note: parameter passing for argument of type [...] changed in GCC 7.1"
dnl
dnl These notices remind that code compiled with GCC 7.1+ are not ABI compatible
dnl with environments built with previous GCC versions. We can remove this block
dnl when the compiler no longer emits these warnings.
dnl
case "$host" in arm-*)
    AC_MSG_WARN([Disabled ARM warnings for GCC 7.1 ABI compatibility.])
    CXXFLAGS="$CXXFLAGS -Wno-psabi"
esac

if test "$enable_debug" = "yes"; then
    if test "$GCC" = "yes"; then
        if test "$TARGET_OS" = "windows"; then
          CFLAGS="$CFLAGS -O1"
        fi
    fi
    if test "$GXX" = "yes"; then
        if test "$TARGET_OS" = "windows"; then
          CXXFLAGS="$CXXFLAGS -O1"
        fi
    fi
fi

if test "$use_extended_functional_tests" != "no"; then
  AC_SUBST(EXTENDED_FUNCTIONAL_TESTS, --extended)
fi

if test "$use_lcov" = "yes"; then
  if test "$LCOV" = ""; then
    AC_MSG_ERROR("lcov testing requested but lcov not found")
  fi
  if test "$GCOV" = ""; then
    AC_MSG_ERROR("lcov testing requested but gcov not found")
  fi
  if test "$PYTHON" = ""; then
    AC_MSG_ERROR("lcov testing requested but python not found")
  fi
  if test "$GENHTML" = ""; then
    AC_MSG_ERROR("lcov testing requested but genhtml not found")
  fi
  LCOV="$LCOV --gcov-tool=$GCOV"
  AX_CHECK_LINK_FLAG([[--coverage]], [LDFLAGS="$LDFLAGS --coverage"],
    [AC_MSG_ERROR("lcov testing requested but --coverage linker flag does not work")])
  AX_CHECK_COMPILE_FLAG([--coverage],[CXXFLAGS="$CXXFLAGS --coverage"],
    [AC_MSG_ERROR("lcov testing requested but --coverage flag does not work")])
  AC_DEFINE(USE_COVERAGE, 1, [Define this symbol if coverage is enabled])
  CXXFLAGS="$CXXFLAGS -Og"
fi

if test "$use_lcov_branch" != "no"; then
  AC_SUBST(LCOV_OPTS, "$LCOV_OPTS --rc lcov_branch_coverage=1")
fi

dnl Check for endianness
AC_C_BIGENDIAN

dnl Check for pthread compile/link requirements
AX_PTHREAD

# The following macro will add the necessary defines to gridcoin-config.h, but
# they also need to be passed down to any subprojects. Pull the results out of
# the cache and add them to CPPFLAGS.
AC_SYS_LARGEFILE
# detect POSIX or GNU variant of strerror_r
AC_FUNC_STRERROR_R

if test "$ac_cv_sys_file_offset_bits" != "" &&
   test "$ac_cv_sys_file_offset_bits" != "no" &&
   test "$ac_cv_sys_file_offset_bits" != "unknown"; then
  CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=$ac_cv_sys_file_offset_bits"
fi

if test "$ac_cv_sys_large_files" != "" &&
   test "$ac_cv_sys_large_files" != "no" &&
   test "$ac_cv_sys_large_files" != "unknown"; then
  CPPFLAGS="$CPPFLAGS -D_LARGE_FILES=$ac_cv_sys_large_files"
fi

AX_CHECK_LINK_FLAG([[-Wl,--large-address-aware]], [LDFLAGS="$LDFLAGS -Wl,--large-address-aware"])

AX_GCC_FUNC_ATTRIBUTE([visibility])
AX_GCC_FUNC_ATTRIBUTE([dllexport])
AX_GCC_FUNC_ATTRIBUTE([dllimport])

AC_SEARCH_LIBS([clock_gettime],[rt])

if test "$TARGET_OS" != "windows"; then
  # All windows code is PIC, forcing it on just adds useless compile warnings
  AX_CHECK_COMPILE_FLAG([-fPIC],[PIC_FLAGS="-fPIC"])
fi

# All versions of gcc that we commonly use for building are subject to bug
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90348. To work around that, set
# -fstack-reuse=none for all gcc builds. (Only gcc understands this flag)
AX_CHECK_COMPILE_FLAG([-fstack-reuse=none],[HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -fstack-reuse=none"])
if test "$use_hardening" != "no"; then
  AX_CHECK_COMPILE_FLAG([-Wstack-protector],[HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -Wstack-protector"])
  AX_CHECK_COMPILE_FLAG([-fstack-protector-all],[HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -fstack-protector-all"])

  case $host in
    i686-w64-mingw32)
      dnl Control-Flow Enforcement Technology (CET) does not appear to work for mingw for i686 (32-bit)
      dnl When it is enabled, unstable executables result. Disable for now.
      ;;
    *)
      AX_CHECK_COMPILE_FLAG([-fcf-protection=full],[HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -fcf-protection=full"])
      ;;
  esac

  case $host in
    *mingw*)
      dnl stack-clash-protection doesn't compile with GCC 10 and earlier.
      dnl In any case, it is a no-op for Windows.
      dnl See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90458 for more details.
      ;;
    *)
      AX_CHECK_COMPILE_FLAG([-fstack-clash-protection], [HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -fstack-clash-protection"], [], [$CXXFLAG_WERROR])
      ;;
  esac

  AX_CHECK_PREPROC_FLAG([-D_FORTIFY_SOURCE=2],[
    AX_CHECK_PREPROC_FLAG([-U_FORTIFY_SOURCE],[
      HARDENED_CPPFLAGS="$HARDENED_CPPFLAGS -U_FORTIFY_SOURCE"
    ])
    HARDENED_CPPFLAGS="$HARDENED_CPPFLAGS -D_FORTIFY_SOURCE=2"
  ])

  AX_CHECK_LINK_FLAG([[-Wl,--dynamicbase]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,--dynamicbase"])
  AX_CHECK_LINK_FLAG([[-Wl,--nxcompat]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,--nxcompat"])
  AX_CHECK_LINK_FLAG([[-Wl,--high-entropy-va]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,--high-entropy-va"])
  AX_CHECK_LINK_FLAG([[-Wl,-z,relro]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,-z,relro"])
  AX_CHECK_LINK_FLAG([[-Wl,-z,now]], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,-z,now"])
  AX_CHECK_LINK_FLAG([[-fPIE -pie]], [PIE_FLAGS="-fPIE"; HARDENED_LDFLAGS="$HARDENED_LDFLAGS -pie"],, [[$CXXFLAG_WERROR]])

  case $host in
    *mingw*)
       AC_CHECK_LIB([ssp],      [main],, AC_MSG_ERROR(lib missing))
    ;;
  esac
fi

dnl this flag screws up non-darwin gcc even when the check fails. special-case it.
if test "$TARGET_OS" = "darwin"; then
  AX_CHECK_LINK_FLAG([[-Wl,-dead_strip]], [LDFLAGS="$LDFLAGS -Wl,-dead_strip"])
fi

AC_CHECK_HEADERS([endian.h sys/endian.h byteswap.h stdio.h stdlib.h unistd.h strings.h sys/types.h sys/stat.h sys/select.h sys/prctl.h])

AC_CHECK_DECLS([strnlen])

dnl These are used for daemonization in bitcoind
AC_CHECK_DECLS([fork])
AC_CHECK_DECLS([setsid])

AC_CHECK_DECLS([pipe2])

AC_CHECK_DECLS([le16toh, le32toh, le64toh, htole16, htole32, htole64, be16toh, be32toh, be64toh, htobe16, htobe32, htobe64],,,
		[#if HAVE_ENDIAN_H
                 #include <endian.h>
                 #elif HAVE_SYS_ENDIAN_H
                 #include <sys/endian.h>
                 #endif])

AC_CHECK_DECLS([bswap_16, bswap_32, bswap_64],,,
		[#if HAVE_BYTESWAP_H
                 #include <byteswap.h>
                 #endif])

AC_MSG_CHECKING(for __builtin_clzl)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ ]], [[
 (void) __builtin_clzl(0);
  ]])],
 [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_BUILTIN_CLZL, 1, [Define this symbol if you have __builtin_clzl])],
 [ AC_MSG_RESULT(no)]
)

AC_MSG_CHECKING(for __builtin_clzll)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ ]], [[
  (void) __builtin_clzll(0);
  ]])],
 [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_BUILTIN_CLZLL, 1, [Define this symbol if you have __builtin_clzll])],
 [ AC_MSG_RESULT(no)]
)

dnl Check for MSG_NOSIGNAL
AC_MSG_CHECKING(for MSG_NOSIGNAL)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/socket.h>]],
 [[ int f = MSG_NOSIGNAL; ]])],
 [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_MSG_NOSIGNAL, 1,[Define this symbol if you have MSG_NOSIGNAL]) ],
 [ AC_MSG_RESULT(no)]
)

dnl Check for MSG_DONTWAIT
AC_MSG_CHECKING(for MSG_DONTWAIT)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/socket.h>]],
 [[ int f = MSG_DONTWAIT; ]])],
 [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_MSG_DONTWAIT, 1,[Define this symbol if you have MSG_DONTWAIT]) ],
 [ AC_MSG_RESULT(no)]
)

dnl Check for malloc_info (for memory statistics information in getmemoryinfo)
AC_MSG_CHECKING(for getmemoryinfo)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <malloc.h>]],
 [[ int f = malloc_info(0, NULL); ]])],
 [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_MALLOC_INFO, 1,[Define this symbol if you have malloc_info]) ],
 [ AC_MSG_RESULT(no)]
)

dnl Check for mallopt(M_ARENA_MAX) (to set glibc arenas)
AC_MSG_CHECKING(for mallopt M_ARENA_MAX)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <malloc.h>]],
 [[ mallopt(M_ARENA_MAX, 1); ]])],
 [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_MALLOPT_ARENA_MAX, 1,[Define this symbol if you have mallopt with M_ARENA_MAX]) ],
 [ AC_MSG_RESULT(no)]
)

AC_MSG_CHECKING([for visibility attribute])
AC_LINK_IFELSE([AC_LANG_SOURCE([
  int foo_def( void ) __attribute__((visibility("default")));
  int main(){}
  ])],
  [
    AC_DEFINE(HAVE_VISIBILITY_ATTRIBUTE,1,[Define if the visibility attribute is supported.])
    AC_MSG_RESULT(yes)
  ],
  [
    AC_MSG_RESULT(no)
    if test "$use_reduce_exports" = "yes"; then
      AC_MSG_ERROR([Cannot find a working visibility attribute. Use --disable-reduce-exports.])
    fi
  ]
)

have_any_system=no
AC_MSG_CHECKING([for std::system])
AC_LINK_IFELSE(
    [ AC_LANG_PROGRAM(
        [[ #include <cstdlib> ]],
        [[ int nErr = std::system(""); ]]
    )],
    [ AC_MSG_RESULT(yes); have_any_system=yes],
    [ AC_MSG_RESULT(no) ]
)

AC_MSG_CHECKING([for ::_wsystem])
AC_LINK_IFELSE(
    [ AC_LANG_PROGRAM(
        [[ ]],
        [[ int nErr = ::_wsystem(""); ]]
    )],
    [ AC_MSG_RESULT(yes); have_any_system=yes],
    [ AC_MSG_RESULT(no) ]
)

if test "x$have_any_system" != "xno"; then
  AC_DEFINE(HAVE_SYSTEM, 1, Define to 1 if std::system or ::wsystem is available.)
fi

dnl Our minimum supported glibc is 2.17, however support for thread_local
dnl did not arrive in glibc until 2.18.
if test "$use_thread_local" = "yes" || test "$use_thread_local" = "auto"; then
  TEMP_LDFLAGS="$LDFLAGS"
  LDFLAGS="$TEMP_LDFLAGS $PTHREAD_CFLAGS"
  AC_MSG_CHECKING([for thread_local support])
  AC_LINK_IFELSE([AC_LANG_SOURCE([
    #include <thread>
    static thread_local int foo = 0;
    static void run_thread() { foo++;}
    int main(){
    for(int i = 0; i < 10; i++) { std::thread(run_thread).detach();}
    return foo;
    }
    ])],
    [
     case $host in
       *mingw*)
          dnl mingw32's implementation of thread_local has also been shown to behave
          dnl erroneously under concurrent usage; see:
          dnl https://gist.github.com/jamesob/fe9a872051a88b2025b1aa37bfa98605
          AC_MSG_RESULT(no)
          ;;
        *freebsd*)
          dnl FreeBSD's implementation of thread_local is also buggy (per
          dnl https://groups.google.com/d/msg/bsdmailinglist/22ncTZAbDp4/Dii_pII5AwAJ)
          AC_MSG_RESULT(no)
          ;;
        *)
          AC_DEFINE(HAVE_THREAD_LOCAL,1,[Define if thread_local is supported.])
          AC_MSG_RESULT(yes)
          ;;
      esac
    ],
    [
      AC_MSG_RESULT(no)
    ]
  )
  LDFLAGS="$TEMP_LDFLAGS"
fi

dnl check for gmtime_r(), fallback to gmtime_s() if that is unavailable
dnl fail if neither are available.
AC_MSG_CHECKING(for gmtime_r)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <ctime>]],
  [[ gmtime_r((const time_t *) nullptr, (struct tm *) nullptr); ]])],
  [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_GMTIME_R, 1, [Define this symbol if gmtime_r is available]) ],
  [ AC_MSG_RESULT(no);
    AC_MSG_CHECKING(for gmtime_s);
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <ctime>]],
       [[ gmtime_s((struct tm *) nullptr, (const time_t *) nullptr); ]])],
       [ AC_MSG_RESULT(yes)],
       [ AC_MSG_RESULT(no); AC_MSG_ERROR(Both gmtime_r and gmtime_s are unavailable) ]
    )
  ]
)

# Check for different ways of gathering OS randomness
AC_MSG_CHECKING(for Linux getrandom syscall)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <unistd.h>
  #include <sys/syscall.h>
  #include <linux/random.h>]],
 [[ syscall(SYS_getrandom, nullptr, 32, 0); ]])],
 [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_SYS_GETRANDOM, 1,[Define this symbol if the Linux getrandom system call is available]) ],
 [ AC_MSG_RESULT(no)]
)

AC_MSG_CHECKING(for sysctl KERN_ARND)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
  #include <sys/sysctl.h>]],
 [[ static const int name[2] = {CTL_KERN, KERN_ARND};
    sysctl(name, 2, nullptr, nullptr, nullptr, 0); ]])],
 [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_SYSCTL_ARND, 1,[Define this symbol if the BSD sysctl(KERN_ARND) is available]) ],
 [ AC_MSG_RESULT(no)]
)

# LevelDB platform checks
AC_MSG_CHECKING(for fdatasync)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <unistd.h>]],
 [[ fdatasync(0); ]])],
 [ AC_MSG_RESULT(yes); HAVE_FDATASYNC=1 ],
 [ AC_MSG_RESULT(no); HAVE_FDATASYNC=0 ]
)

AC_MSG_CHECKING(for F_FULLFSYNC)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <fcntl.h>]],
 [[ fcntl(0, F_FULLFSYNC, 0); ]])],
 [ AC_MSG_RESULT(yes); HAVE_FULLFSYNC=1 ],
 [ AC_MSG_RESULT(no); HAVE_FULLFSYNC=0 ]
)

AC_MSG_CHECKING(for O_CLOEXEC)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <fcntl.h>]],
 [[ open("", O_CLOEXEC); ]])],
 [ AC_MSG_RESULT(yes); HAVE_O_CLOEXEC=1 ],
 [ AC_MSG_RESULT(no); HAVE_O_CLOEXEC=0 ]
)

# crc32c platform checks
AC_MSG_CHECKING(for __builtin_prefetch)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ ]], [[
  char data = 0;
  const char* address = &data;
  __builtin_prefetch(address, 0, 0);
  ]])],
 [ AC_MSG_RESULT(yes); HAVE_BUILTIN_PREFETCH=1 ],
 [ AC_MSG_RESULT(no); HAVE_BUILTIN_PREFETCH=0 ]
)

AC_MSG_CHECKING(for _mm_prefetch)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <xmmintrin.h>]], [[
  char data = 0;
  const char* address = &data;
  _mm_prefetch(address, _MM_HINT_NTA);
  ]])],
 [ AC_MSG_RESULT(yes); HAVE_MM_PREFETCH=1 ],
 [ AC_MSG_RESULT(no); HAVE_MM_PREFETCH=0 ]
)

AC_MSG_CHECKING(for strong getauxval support in the system headers)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <arm_acle.h>
    #include <arm_neon.h>
    #include <sys/auxv.h>
  ]], [[
    getauxval(AT_HWCAP);
  ]])],
 [ AC_MSG_RESULT(yes); HAVE_STRONG_GETAUXVAL=1 ],
 [ AC_MSG_RESULT(no); HAVE_STRONG_GETAUXVAL=0 ]
)

AC_MSG_CHECKING(for weak getauxval support in the compiler)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    unsigned long getauxval(unsigned long type) __attribute__((weak));
    #define AT_HWCAP 16
  ]], [[
    getauxval(AT_HWCAP);
  ]])],
 [ AC_MSG_RESULT(yes); HAVE_WEAK_GETAUXVAL=1 ],
 [ AC_MSG_RESULT(no); HAVE_WEAK_GETAUXVAL=0 ]
)

# Check for reduced exports
if test "$use_reduce_exports" = "yes"; then
  AX_CHECK_COMPILE_FLAG([-fvisibility=hidden],[RE_CXXFLAGS="-fvisibility=hidden"],
  [AC_MSG_ERROR([Cannot set default symbol visibility. Use --disable-reduce-exports.])])
fi

dnl Check for libminiupnpc (optional)
if test "$use_upnp" != "no"; then
  AC_CHECK_HEADERS(
    [miniupnpc/miniwget.h miniupnpc/miniupnpc.h miniupnpc/upnpcommands.h miniupnpc/upnperrors.h],
    [AC_CHECK_LIB([miniupnpc], [main],[MINIUPNPC_LIBS=-lminiupnpc], [have_miniupnpc=no])],
    [have_miniupnpc=no]
  )
dnl The minimum supported miniUPnPc API version is set to 10. This keeps compatibility
dnl with Ubuntu 16.04 LTS and Debian 8 libminiupnpc-dev packages.
if test "$have_miniupnpc" != "no"; then
  AC_MSG_CHECKING([whether miniUPnPc API version is supported])
  AC_PREPROC_IFELSE([AC_LANG_PROGRAM([[
      @%:@include <miniupnpc/miniupnpc.h>
    ]], [[
      #if MINIUPNPC_API_VERSION >= 10
      // Everything is okay
      #else
      #  error miniUPnPc API version is too old
      #endif
    ]])],[
      AC_MSG_RESULT(yes)
    ],[
    AC_MSG_RESULT(no)
    AC_MSG_WARN([miniUPnPc API version < 10 is unsupported, disabling UPnP support.])
    have_miniupnpc=no
  ])
fi
fi


BITCOIN_QT_INIT

dnl sets $bitcoin_enable_qt, $bitcoin_enable_qt_test, $bitcoin_enable_qt_dbus
BITCOIN_QT_CONFIGURE([5.9.5])

if test "$build_bitcoin_utils$build_gridcoinresearchd$bitcoin_enable_qt$use_tests$use_bench" = "nonononono"; then
    use_boost=no
else
    use_boost=yes
fi

if test "$use_boost" = "yes"; then

dnl Minimum required Boost version
define(MINIMUM_REQUIRED_BOOST, 1.60.0)

dnl Check for Boost libs
AX_BOOST_BASE([MINIMUM_REQUIRED_BOOST])
AX_BOOST_SYSTEM
AX_BOOST_FILESYSTEM
AX_BOOST_ZLIB
AX_BOOST_IOSTREAMS

dnl Prevent use of std::unary_function, which was removed in C++17,
dnl and will generate warnings with newer compilers for Boost
dnl older than 1.80.
dnl See: https://github.com/boostorg/config/pull/430.
AX_CHECK_PREPROC_FLAG([-DBOOST_NO_CXX98_FUNCTION_BASE], [BOOST_CPPFLAGS="$BOOST_CPPFLAGS -DBOOST_NO_CXX98_FUNCTION_BASE"], [], [$CXXFLAG_WERROR],
                      [AC_LANG_PROGRAM([[#include <boost/config.hpp>]])])

dnl AX_BOOST_THREAD check also triggers the same issue, so it has to be checked after
dnl setting -DBOOST_NO_CXX98_FUNCTION_BASE.
AX_BOOST_THREAD

dnl Boost 1.56 through 1.62 allow using std::atomic instead of its own atomic
dnl counter implementations. In 1.63 and later the std::atomic approach is default.
m4_pattern_allow(DBOOST_AC_USE_STD_ATOMIC) dnl otherwise it's treated like a macro
BOOST_CPPFLAGS="-DBOOST_SP_USE_STD_ATOMIC -DBOOST_AC_USE_STD_ATOMIC $BOOST_CPPFLAGS"

BOOST_LIBS="$BOOST_LDFLAGS $BOOST_SYSTEM_LIB $BOOST_FILESYSTEM_LIB $BOOST_IOSTREAMS_LIB $BOOST_THREAD_LIB $BOOST_ZLIB_LIB"
fi

if test "$use_reduce_exports" = "yes"; then
    CXXFLAGS="$CXXFLAGS $RE_CXXFLAGS"
    AX_CHECK_LINK_FLAG([[-Wl,--exclude-libs,ALL]], [RELDFLAGS="-Wl,--exclude-libs,ALL"])
fi

if test "$use_tests" = "yes"; then

  if test "$HEXDUMP" = ""; then
    AC_MSG_ERROR(hexdump is required for tests)
  fi


  if test "$use_boost" = "yes"; then

  AX_BOOST_UNIT_TEST_FRAMEWORK

  dnl Determine if -DBOOST_TEST_DYN_LINK is needed
  AC_MSG_CHECKING([for dynamic linked boost test])
  TEMP_LIBS="$LIBS"
  LIBS="$LIBS $BOOST_LDFLAGS $BOOST_UNIT_TEST_FRAMEWORK_LIB"
  TEMP_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
  AC_LINK_IFELSE([AC_LANG_SOURCE([
       #define BOOST_TEST_DYN_LINK
       #define BOOST_TEST_MAIN
        #include <boost/test/unit_test.hpp>

       ])],
    [AC_MSG_RESULT(yes)]
    [TESTDEFS="$TESTDEFS -DBOOST_TEST_DYN_LINK"],
    [AC_MSG_RESULT(no)])
  LIBS="$TEMP_LIBS"
  CPPFLAGS="$TEMP_CPPFLAGS"

  fi
fi

PKG_CHECK_MODULES([SSL], [libssl],, [AC_MSG_ERROR(openssl not found.)])
PKG_CHECK_MODULES([CRYPTO], [libcrypto],,[AC_MSG_ERROR(libcrypto not found.)])
PKG_CHECK_MODULES([LIBZIP], [libzip],,[AC_MSG_ERROR(libzip not found.)])
PKG_CHECK_MODULES([CURL], [libcurl],,[AC_MSG_ERROR(libcurl not found.)])

if test "$use_qr" != "no"; then
  BITCOIN_QT_CHECK([PKG_CHECK_MODULES([QR], [libqrencode], [have_qrencode=yes], [have_qrencode=no])])
fi

dnl univalue check

need_bundled_univalue=yes
UNIVALUE_CFLAGS='-I$(srcdir)/univalue/include'
UNIVALUE_LIBS='univalue/libunivalue.la'

AM_CONDITIONAL([EMBEDDED_UNIVALUE],[test "$need_bundled_univalue" = "yes"])
AC_SUBST(UNIVALUE_CFLAGS)
AC_SUBST(UNIVALUE_LIBS)


save_CXXFLAGS="${CXXFLAGS}"
CXXFLAGS="${CXXFLAGS} ${CRYPTO_CFLAGS} ${SSL_CFLAGS}"
AC_CHECK_DECLS([EVP_MD_CTX_new],,,[AC_INCLUDES_DEFAULT
#include <openssl/x509_vfy.h>
])
CXXFLAGS="${save_CXXFLAGS}"

AC_MSG_CHECKING([whether to build gridcoinresearchd])
AM_CONDITIONAL([BUILD_GRIDCOINRESEARCHD], [test "$build_gridcoinresearchd" = "yes"])
AC_MSG_RESULT($build_gridcoinresearchd)

AC_MSG_CHECKING([whether to build utils (bitcoin-cli bitcoin-tx)])
AM_CONDITIONAL([BUILD_BITCOIN_UTILS], [test "$build_bitcoin_utils" = "yes"])
AC_MSG_RESULT($build_bitcoin_utils)

AC_LANG_POP

if test "$use_ccache" != "no"; then
  AC_MSG_CHECKING(if ccache should be used)
  if test "$CCACHE" = ""; then
    if test "$use_ccache" = "yes"; then
      AC_MSG_ERROR([ccache not found.]);
    else
      use_ccache=no
    fi
  else
    use_ccache=yes
    CC="$ac_cv_path_CCACHE $CC"
    CXX="$ac_cv_path_CCACHE $CXX"
  fi
  AC_MSG_RESULT($use_ccache)
fi
if test "$use_ccache" = "yes"; then
    AX_CHECK_PREPROC_FLAG([-Qunused-arguments],[CPPFLAGS="-Qunused-arguments $CPPFLAGS"])
fi

dnl enable wallet
AC_MSG_CHECKING([if wallet should be enabled])
if test "$enable_wallet" != "no"; then
  AC_MSG_RESULT(yes)
  AC_DEFINE_UNQUOTED([ENABLE_WALLET],[1],[Define to 1 to enable wallet functions])

else
  AC_MSG_RESULT(no)
fi

dnl enable upnp support
AC_MSG_CHECKING([whether to build with support for UPnP])
if test "$have_miniupnpc" = "no"; then
  if test "$use_upnp" = "yes"; then
     AC_MSG_ERROR("UPnP requested but cannot be built. Use --without-miniupnpc.")
  fi
  AC_MSG_RESULT(no)
else
  if test "$use_upnp" != "no"; then
    AC_MSG_RESULT(yes)
    AC_MSG_CHECKING([whether to build with UPnP enabled by default])
    use_upnp=yes
    upnp_setting=0
    if test "$use_upnp_default" != "no"; then
      use_upnp_default=yes
      upnp_setting=1
    fi
    AC_MSG_RESULT($use_upnp_default)
    AC_DEFINE_UNQUOTED([USE_UPNP],[$upnp_setting],[UPnP support not compiled if undefined, otherwise value (0 or 1) determines default state])
    if test "$TARGET_OS" = "windows"; then
      MINIUPNPC_CPPFLAGS="-DSTATICLIB -DMINIUPNP_STATICLIB"
    fi
  else
    AC_MSG_RESULT(no)
  fi
fi

dnl these are only used when qt is enabled
BUILD_TEST_QT=""
if test "$bitcoin_enable_qt" != "no"; then
  CPPFLAGS="$CPPFLAGS -DQT_GUI"

  dnl enable dbus support
  AC_MSG_CHECKING([whether to build GUI with support for D-Bus])
  if test "$bitcoin_enable_qt_dbus" != "no"; then
    AC_DEFINE([USE_DBUS],[1],[Define if dbus support should be compiled in])
  fi
  AC_MSG_RESULT($bitcoin_enable_qt_dbus)

  dnl enable qr support
  AC_MSG_CHECKING([whether to build GUI with support for QR codes])
  if test "$have_qrencode" = "no"; then
    if test "$use_qr" = "yes"; then
     AC_MSG_ERROR("QR support requested but cannot be built. use --without-qrencode")
    fi
    AC_MSG_RESULT(no)
  else
    if test "$use_qr" != "no"; then
      AC_MSG_RESULT(yes)
      AC_DEFINE([USE_QRCODE],[1],[Define if QR support should be compiled in])
      use_qr=yes
      CPPFLAGS="$CPPFLAGS -DUSE_QRCODE"
    else
      AC_MSG_RESULT(no)
    fi
  fi

  if test "$XGETTEXT" = ""; then
    AC_MSG_WARN("xgettext is required to update qt translations")
  fi

  AC_MSG_CHECKING([whether to build test_bitcoin-qt])
  if test "$use_gui_tests$bitcoin_enable_qt_test" = "yesyes"; then
    AC_MSG_RESULT([yes])
    BUILD_TEST_QT="yes"
  else
    AC_MSG_RESULT([no])
  fi
fi

AM_CONDITIONAL([ENABLE_ZMQ], [test "$use_zmq" = "yes"])

AC_MSG_CHECKING([whether to build test_bitcoin])
if test "$use_tests" = "yes"; then
  AC_MSG_RESULT([yes])
  BUILD_TEST="yes"
else
  AC_MSG_RESULT([no])
  BUILD_TEST=""
fi

AC_MSG_CHECKING([whether to reduce exports])
if test "$use_reduce_exports" = "yes"; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([if embedded bdb should be used])
if test "$enable_embedded_bdb" = "yes"; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
  AC_MSG_CHECKING([if bdb 5.3 links])
  TEMP_CXXFLAGS="$CXXFLAGS"
  TEMP_LDFLAGS="$LDFLAGS"
  CXXFLAGS="$CXXFLAGS $BDB_CFLAGS"
  LDFLAGS="$LDFLAGS $BDB_LIBS"
  AC_LANG_PUSH([C++])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
    #include <db_cxx.h>]],[[
    #if !(DB_VERSION_MAJOR == 5 && DB_VERSION_MINOR == 3)
      #error "unsupported bdb version"
    #endif
    Db* db = nullptr;
    ]])],[AC_MSG_RESULT(yes)],[AC_MSG_ERROR([bdb 5.3 does not properly link])])
  AC_LANG_POP([C++])
  CXXFLAGS="$TEMP_CXXFLAGS"
  LDFLAGS="$TEMP_LDFLAGS"
fi
AM_CONDITIONAL([EMBEDDED_BDB], [test "$enable_embedded_bdb" = "yes"])

if test "$build_bitcoin_utils$build_bitcoin_libs$build_gridcoinresearchd$bitcoin_enable_qt$use_bench$use_tests" = "nononononono"; then
  AC_MSG_ERROR([No targets! Please specify at least one of: --with-utils --with-libs --with-daemon --with-gui --enable-bench or --enable-tests])
fi

AM_CONDITIONAL([TARGET_DARWIN], [test "$TARGET_OS" = "darwin"])
AM_CONDITIONAL([BUILD_DARWIN], [test "$BUILD_OS" = "darwin"])
AM_CONDITIONAL([TARGET_WINDOWS], [test "$TARGET_OS" = "windows"])
AM_CONDITIONAL([ENABLE_WALLET],[test "$enable_wallet" = "yes"])
AM_CONDITIONAL([ENABLE_TESTS],[test "$BUILD_TEST" = "yes"])
AM_CONDITIONAL([ENABLE_QT],[test "$bitcoin_enable_qt" = "yes"])
AM_CONDITIONAL([ENABLE_QT_TESTS],[test "$BUILD_TEST_QT" = "yes"])
AM_CONDITIONAL([ENABLE_BENCH],[test "$use_bench" = "yes"])
AM_CONDITIONAL([USE_QRCODE], [test "$use_qr" = "yes"])
AM_CONDITIONAL([USE_LCOV],[test "$use_lcov" = "yes"])
AM_CONDITIONAL([HARDEN],[test "$use_hardening" = "yes"])
AM_CONDITIONAL([ENABLE_SSE42],[test "$enable_sse42" = "yes"])
AM_CONDITIONAL([ENABLE_SSE41],[test "$enable_sse41" = "yes"])
AM_CONDITIONAL([ENABLE_AVX2],[test "$enable_avx2" = "yes"])
AM_CONDITIONAL([ENABLE_X86_SHANI], [test "$enable_x86_shani" = "yes"])
AM_CONDITIONAL([ENABLE_ARM_CRC],[test "$enable_arm_crc" = "yes"])
AM_CONDITIONAL([ENABLE_ARM_SHANI], [test "$enable_arm_shani" = "yes"])
AM_CONDITIONAL([USE_ASM],[test "$use_asm" = "yes"])
AM_CONDITIONAL([WORDS_BIGENDIAN],[test "$ac_cv_c_bigendian" = "yes"])
AM_CONDITIONAL([ARCH_x86], [test "$have_x86" = "true"])
AM_CONDITIONAL([ARCH_x86_64], [test "$have_x86_64" = "true"])
AM_CONDITIONAL([ARCH_ARM], [test "$have_arm" = "true"])

AC_DEFINE(CLIENT_VERSION_MAJOR, _CLIENT_VERSION_MAJOR, [Major version])
AC_DEFINE(CLIENT_VERSION_MINOR, _CLIENT_VERSION_MINOR, [Minor version])
AC_DEFINE(CLIENT_VERSION_REVISION, _CLIENT_VERSION_REVISION, [Build revision])
AC_DEFINE(CLIENT_VERSION_BUILD, _CLIENT_VERSION_BUILD, [Version Build])
AC_DEFINE(CLIENT_VERSION_IS_RELEASE, _CLIENT_VERSION_IS_RELEASE, [Version is release])
AC_DEFINE(COPYRIGHT_YEAR, _COPYRIGHT_YEAR, [Copyright year])
AC_DEFINE(COPYRIGHT_HOLDERS, "_COPYRIGHT_HOLDERS", [Copyright holder(s) before %s replacement])
AC_DEFINE(COPYRIGHT_HOLDERS_SUBSTITUTION, "_COPYRIGHT_HOLDERS_SUBSTITUTION", [Replacement for %s in copyright holders string])
define(_COPYRIGHT_HOLDERS_FINAL, [patsubst(_COPYRIGHT_HOLDERS, [%s], [_COPYRIGHT_HOLDERS_SUBSTITUTION])])
AC_DEFINE(COPYRIGHT_HOLDERS_FINAL, "_COPYRIGHT_HOLDERS_FINAL", [Copyright holder(s)])
AC_SUBST(CLIENT_VERSION_MAJOR, _CLIENT_VERSION_MAJOR)
AC_SUBST(CLIENT_VERSION_MINOR, _CLIENT_VERSION_MINOR)
AC_SUBST(CLIENT_VERSION_REVISION, _CLIENT_VERSION_REVISION)
AC_SUBST(CLIENT_VERSION_BUILD, _CLIENT_VERSION_BUILD)
AC_SUBST(CLIENT_VERSION_IS_RELEASE, _CLIENT_VERSION_IS_RELEASE)
AC_SUBST(COPYRIGHT_YEAR, _COPYRIGHT_YEAR)
AC_SUBST(COPYRIGHT_HOLDERS, "_COPYRIGHT_HOLDERS")
AC_SUBST(COPYRIGHT_HOLDERS_SUBSTITUTION, "_COPYRIGHT_HOLDERS_SUBSTITUTION")
AC_SUBST(COPYRIGHT_HOLDERS_FINAL, "_COPYRIGHT_HOLDERS_FINAL")
AC_SUBST(GRIDCOIN_DAEMON_NAME)
AC_SUBST(GRIDCOIN_GUI_NAME)

AC_SUBST(RELDFLAGS)
AC_SUBST(ERROR_CXXFLAGS)
AC_SUBST(HARDENED_CXXFLAGS)
AC_SUBST(HARDENED_CPPFLAGS)
AC_SUBST(HARDENED_LDFLAGS)
AC_SUBST(PIC_FLAGS)
AC_SUBST(PIE_FLAGS)
AC_SUBST(SANITIZER_CXXFLAGS)
AC_SUBST(SANITIZER_LDFLAGS)
AC_SUBST(SSE42_CXXFLAGS)
AC_SUBST(SSE41_CXXFLAGS)
AC_SUBST(AVX2_CXXFLAGS)
AC_SUBST(X86_SHANI_CXXFLAGS)
AC_SUBST(ARM_CRC_CXXFLAGS)
AC_SUBST(ARM_SHANI_CXXFLAGS)
AC_SUBST(LIBTOOL_APP_LDFLAGS)
AC_SUBST(USE_UPNP)
AC_SUBST(USE_QRCODE)
AC_SUBST(BOOST_LIBS)
AC_SUBST(TESTDEFS)
AC_SUBST(MINIUPNPC_CPPFLAGS)
AC_SUBST(MINIUPNPC_LIBS)
AC_SUBST(CRYPTO_LIBS)
AC_SUBST(SSL_LIBS)
AC_SUBST(CURL_LIBS)
AC_SUBST(LIBZIP_LIBS)
AC_SUBST(EVENT_LIBS)
AC_SUBST(EVENT_PTHREADS_LIBS)
AC_SUBST(ZMQ_LIBS)
AC_SUBST(PROTOBUF_LIBS)
AC_SUBST(QR_LIBS)
AC_SUBST(BDB_CFLAGS)
AC_SUBST(BDB_LIBS)
AC_SUBST(HAVE_FDATASYNC)
AC_SUBST(HAVE_FULLFSYNC)
AC_SUBST(HAVE_O_CLOEXEC)
AC_SUBST(HAVE_BUILTIN_PREFETCH)
AC_SUBST(HAVE_MM_PREFETCH)
AC_SUBST(HAVE_STRONG_GETAUXVAL)
AC_SUBST(HAVE_WEAK_GETAUXVAL)
AC_CONFIG_FILES([Makefile src/Makefile share/setup.nsi share/qt/Info.plist])
AC_CONFIG_FILES([contrib/devtools/split-debug.sh],[chmod +x contrib/devtools/split-debug.sh])
AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([doc/Doxyfile])])

dnl boost's m4 checks do something really nasty: they export these vars. As a
dnl result, they leak into secp256k1's configure and crazy things happen.
dnl Until this is fixed upstream and we've synced, we'll just un-export them.
CPPFLAGS_TEMP="$CPPFLAGS"
unset CPPFLAGS
CPPFLAGS="$CPPFLAGS_TEMP"

LDFLAGS_TEMP="$LDFLAGS"
unset LDFLAGS
LDFLAGS="$LDFLAGS_TEMP"

LIBS_TEMP="$LIBS"
unset LIBS
LIBS="$LIBS_TEMP"

PKGCONFIG_PATH_TEMP="$PKG_CONFIG_PATH"
unset PKG_CONFIG_PATH
PKG_CONFIG_PATH="$PKGCONFIG_PATH_TEMP"

PKGCONFIG_LIBDIR_TEMP="$PKG_CONFIG_LIBDIR"
unset PKG_CONFIG_LIBDIR
PKG_CONFIG_LIBDIR="$PKGCONFIG_LIBDIR_TEMP"

ac_configure_args="${ac_configure_args} --disable-shared --with-pic --with-bignum=no --enable-module-recovery --disable-module-ecdh"

ADDITIONAL_BDB_FLAGS=""

case $host in
    *mingw*)
        ADDITIONAL_BDB_FLAGS="--enable-mingw"
    ;;
esac

AC_CONFIG_SUBDIRS([src/univalue])
AC_CONFIG_SUBDIRS([src/secp256k1])
if test "$enable_embedded_bdb" = "yes"; then
  AX_SUBDIRS_CONFIGURE([src/bdb53], [[CFLAGS="-Wno-error=implicit-function-declaration"], [CXXFLAGS="-Wno-error=implicit-function-declaration"], [--disable-java], [--disable-jdbc], [--disable-replication], [--enable-cxx], [$ADDITIONAL_BDB_FLAGS]], [], [], [])
fi

AC_OUTPUT

dnl Taken from https://wiki.debian.org/RpathIssue
case $host in
   *-*-linux-gnu)
     AC_MSG_RESULT([Fixing libtool for -rpath problems.])
     sed < libtool > libtool-2 \
     's/^hardcode_libdir_flag_spec.*$'/'hardcode_libdir_flag_spec=" -D__LIBTOOL_IS_A_FOOL__ "/'
     mv libtool-2 libtool
     chmod 755 libtool
   ;;
esac

dnl Replace the BUILDDIR path with the correct Windows path if compiling on Native Windows
case ${OS} in
   *Windows*)
     sed  's/BUILDDIR="\/\([[a-z]]\)/BUILDDIR="\1:/'  test/config.ini > test/config-2.ini
     mv test/config-2.ini test/config.ini
   ;;
esac

echo
echo "Options used to compile and link:"
echo "  with wallet   = $enable_wallet"
echo "  with gui / qt = $bitcoin_enable_qt"
if test "$bitcoin_enable_qt" != "no"; then
    echo "    with qr     = $use_qr"
fi
echo "  with zmq      = $use_zmq"
echo "  with test     = $use_tests"
echo "  with bench    = $use_bench"
echo "  with upnp     = $use_upnp"
echo "  sanitizers    = $use_sanitizers"
echo "  debug enabled = $enable_debug"
echo "  werror        = $enable_werror"
echo
echo "  target os     = $TARGET_OS"
echo "  build os      = $BUILD_OS"
echo
echo "  CC            = $CC"
echo "  CFLAGS        = $CFLAGS"
echo "  CPPFLAGS      = $CPPFLAGS"
echo "  CXX           = $CXX"
echo "  CXXFLAGS      = $CXXFLAGS"
echo "  LDFLAGS       = $LDFLAGS"
echo "  AS            = $CCAS"
echo "  ASFLAGS       = $CCASFLAGS"
echo
