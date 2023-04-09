# CMake build system is intended to be used by maintainers to package Gridcoin
# for software repositories. For that reason, only external dependencies are
# supported.
#
# Use Autotools build system for all other needs.
#
# CMake support is experimental. Use with caution and report any bugs.

cmake_minimum_required(VERSION 3.18)

project("Gridcoin"
    VERSION 5.4.3.1
    DESCRIPTION "POS-based cryptocurrency that rewards BOINC computation"
    HOMEPAGE_URL "https://gridcoin.us"
    LANGUAGES ASM C CXX
)

set(CLIENT_VERSION_IS_RELEASE "false")
set(COPYRIGHT_YEAR "2023")
set(COPYRIGHT_HOLDERS_FINAL "The Gridcoin developers")


# Toolchain configuration
# =======================

set(CMAKE_C_VISIBILITY_PRESET hidden)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Remove '-DNDEBUG' from flags because we need asserts
string(REPLACE "NDEBUG" "_NDEBUG" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "NDEBUG" "_NDEBUG" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")


# Load modules from the source tree
# =================================

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/build-aux/cmake")

include(CheckCXXSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckPIESupported)
include(CheckSSE)
include(CheckStrerrorR)
include(CheckSymbolExists)
include(VersionFromGit)

# Define options
# ==============

# Build configuration
option(ENABLE_DAEMON    "Enable daemon" ON)
option(ENABLE_GUI       "Enable Qt-based GUI" OFF)
option(ENABLE_DOCS      "Build Doxygen documentation" OFF)
option(ENABLE_TESTS     "Build tests" OFF)
option(LUPDATE          "Update translation files" OFF)

# CPU-dependent options
option(ENABLE_SSE41     "Build code that uses SSE4.1 intrinsics" ${HAS_SSE41})
option(ENABLE_AVX2      "Build code that uses AVX2 intrinsics" ${HAS_AVX2})
option(ENABLE_X86_SHANI "Build code that uses x86 SHA-NI intrinsics" ${HAS_X86_SHANI})
option(ENABLE_ARM_SHANI "Build code that uses ARM SHA-NI intrinsics" ${HAS_ARM_SHANI})
option(USE_ASM          "Enable assembly routines" ON)

# Optional functionality
option(ENABLE_QRENCODE  "Enable generation of QR Codes for receiving payments" OFF)
option(ENABLE_UPNP      "Enable UPnP port mapping support" OFF)
option(DEFAULT_UPNP     "Turn UPnP on startup" OFF)
option(USE_DBUS         "Enable DBus support" OFF)
option(SYSTEM_BDB	    "Find system installation of Berkeley DB CXX 5.3" OFF)
option(SYSTEM_LEVELDB   "Find system installation of leveldb" OFF)
option(SYSTEM_SECP256K1 "Find system installation of libsecp256k1 with pkg-config" OFF)
option(SYSTEM_UNIVALUE  "Find system installation of Univalue with pkg-config" OFF)
option(SYSTEM_XXD       "Find system xxd binary" OFF)


# Find dependencies
# =================

set(BOOST_MINIMUM_VERSION 1.63.0)
set(QT5_MINIMUM_VERSION 5.15.0)

find_package(Atomics REQUIRED)
find_package(Boost ${BOOST_MINIMUM_VERSION} COMPONENTS filesystem iostreams thread REQUIRED)
find_package(CURL COMPONENTS HTTP HTTPS SSL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(libzip REQUIRED)

if(SYSTEM_BDB)
    find_package(BerkeleyDB 5.3...<5.4 COMPONENTS CXX REQUIRED)
else()
    find_program(MAKE_EXE NAMES gmake nmake make)
endif()

if(SYSTEM_LEVELDB)
    find_package(leveldb REQUIRED)
endif()

if(SYSTEM_SECP256K1)
    find_package(PkgConfig)
    pkg_check_modules(SECP256K1 REQUIRED IMPORTED_TARGET "libsecp256k1 >= 0.2.0")
endif()

if(SYSTEM_UNIVALUE)
    find_package(PkgConfig)
    pkg_check_modules(UNIVALUE REQUIRED IMPORTED_TARGET libunivalue)
endif()

if(ENABLE_GUI)
    find_package(Qt5 ${QT5_MINIMUM_VERSION} REQUIRED COMPONENTS
        Concurrent
        Core
        Gui
        LinguistTools
        Network
        Widgets
    )

    if(USE_DBUS)
        find_package(Qt5 ${QT5_MINIMUM_VERSION} COMPONENTS DBus REQUIRED)
    endif()

    if(ENABLE_TESTS)
        find_package(Qt5 ${QT5_MINIMUM_VERSION} COMPONENTS Test REQUIRED)
    endif()

    if(ENABLE_QRENCODE)
        pkg_check_modules(QRENCODE REQUIRED IMPORTED_TARGET libqrencode)
    endif()
endif()

if(ENABLE_UPNP)
    pkg_check_modules(MINIUPNPC REQUIRED IMPORTED_TARGET miniupnpc>=1.9)
endif()

if(ENABLE_TESTS)
    find_package(Boost ${BOOST_MINIMUM_VERSION} COMPONENTS unit_test_framework REQUIRED)
    enable_testing()

    if(SYSTEM_XXD)
        find_program(XXD xxd REQUIRED)
    endif()
endif()

if(WIN32)
    enable_language(RC)
    find_program(MAKENSIS makensis)
elseif(APPLE)
    enable_language(OBJCXX)
endif()


# Run probes
# ==========

# Detect whether PIE is supported by the current linker and environment
check_pie_supported(OUTPUT_VARIABLE ENABLE_PIE)

# Set endianness
if(CMAKE_CXX_BYTE_ORDER EQUAL BIG_ENDIAN)
    set(WORDS_BIGENDIAN 1)
endif()

# Check headers
check_include_file("byteswap.h" HAVE_BYTESWAP_H)
check_include_file("endian.h" HAVE_ENDIAN_H)
check_include_file("sys/endian.h" HAVE_SYS_ENDIAN_H)
check_include_file("sys/prctl.h" HAVE_SYS_PRCTL_H)

if(HAVE_ENDIAN_H)
    set(ENDIAN_INCLUDES "endian.h")
else()
    set(ENDIAN_INCLUDES "sys/endian.h")
endif()

if(HAVE_BYTESWAP_H)
    set(BYTESWAP_INCLUDES "byteswap.h")
endif()

# Check symbols
check_symbol_exists(fork "unistd.h" HAVE_DECL_FORK)
check_symbol_exists(pipe2 "unistd.h" HAVE_DECL_PIPE2)
check_symbol_exists(setsid "unistd.h" HAVE_DECL_SETSID)

check_symbol_exists(le16toh "${ENDIAN_INCLUDES}" HAVE_DECL_LE16TOH)
check_symbol_exists(le32toh "${ENDIAN_INCLUDES}" HAVE_DECL_LE32TOH)
check_symbol_exists(le64toh "${ENDIAN_INCLUDES}" HAVE_DECL_LE64TOH)

check_symbol_exists(htole16 "${ENDIAN_INCLUDES}" HAVE_DECL_HTOLE16)
check_symbol_exists(htole32 "${ENDIAN_INCLUDES}" HAVE_DECL_HTOLE32)
check_symbol_exists(htole64 "${ENDIAN_INCLUDES}" HAVE_DECL_HTOLE64)

check_symbol_exists(be16toh "${ENDIAN_INCLUDES}" HAVE_DECL_BE16TOH)
check_symbol_exists(be32toh "${ENDIAN_INCLUDES}" HAVE_DECL_BE32TOH)
check_symbol_exists(be64toh "${ENDIAN_INCLUDES}" HAVE_DECL_BE64TOH)

check_symbol_exists(htobe16 "${ENDIAN_INCLUDES}" HAVE_DECL_HTOBE16)
check_symbol_exists(htobe32 "${ENDIAN_INCLUDES}" HAVE_DECL_HTOBE32)
check_symbol_exists(htobe64 "${ENDIAN_INCLUDES}" HAVE_DECL_HTOBE64)

check_symbol_exists(bswap_16 "${BYTESWAP_INCLUDES}" HAVE_DECL_BSWAP_16)
check_symbol_exists(bswap_32 "${BYTESWAP_INCLUDES}" HAVE_DECL_BSWAP_32)
check_symbol_exists(bswap_64 "${BYTESWAP_INCLUDES}" HAVE_DECL_BSWAP_64)

check_function_exists(__builtin_clzl HAVE_BUILTIN_CLZL)
check_function_exists(__builtin_clzll HAVE_BUILTIN_CLZLL)

check_symbol_exists(MSG_NOSIGNAL "sys/socket.h" HAVE_MSG_NOSIGNAL)
check_symbol_exists(MSG_DONTWAIT "sys/socket.h" HAVE_MSG_DONTWAIT)

check_symbol_exists(malloc_info "malloc.h" HAVE_MALLOC_INFO)
check_symbol_exists(M_ARENA_MAX "malloc.h" HAVE_MALLOPT_ARENA_MAX)

check_cxx_symbol_exists(std::system "cstdlib" HAVE_SYSTEM)
check_cxx_symbol_exists(gmtime_r "ctime" HAVE_GMTIME_R)

if(NOT HAVE_GMTIME_R)
    check_cxx_symbol_exists(gmtime_s "ctime" HAVE_GMTIME_S)
    if(NOT HAVE_GMTIME_S)
        message(FATAL_ERROR "Both gmtime_r and gmtime_s are unavailable")
    endif()
endif()

check_symbol_exists(SYS_getrandom "sys/syscall.h" HAVE_SYS_GETRANDOM)
check_symbol_exists(getentropy "unistd.h" HAVE_GETENTROPY)
check_symbol_exists(KERN_ARND "sys/sysctl.h" HAVE_SYSCTL_ARND)

check_symbol_exists(O_CLOEXEC "fcntl.h" HAVE_O_CLOEXEC)
check_symbol_exists(getauxval "sys/auxv.h" HAVE_STRONG_GETAUXVAL)

# Descend into subdirectories
# ===========================

add_subdirectory(src)
if(ENABLE_DOCS)
    add_subdirectory(doc)
endif()
