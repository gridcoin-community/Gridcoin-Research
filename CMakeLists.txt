# CMake build system is intended to be used by maintainers to package Gridcoin
# for software repositories. For that reason, only external dependencies are
# supported.
#
# Use Autotools build system for all other needs.
#
# CMake support is experimental. Use with caution and report any bugs.

cmake_minimum_required(VERSION 3.18)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/build-aux/cmake")


# Hunter Package Manager
# ======================

# Windows doesn't yet have a package manager that can be used for managing
# dependencies, so we use Hunter on it.
option(HUNTER_ENABLED "Enable Hunter package manager" OFF)
include(HunterGate)
HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/refs/tags/v0.25.3.tar.gz"
    SHA1 "0dfbc2cb5c4cf7e83533733bdfd2125ff96680cb"
    FILEPATH "${CMAKE_CURRENT_SOURCE_DIR}/build-aux/cmake/Hunter/config.cmake"
)


# Project configuration
# =====================

project("Gridcoin"
    VERSION 5.4.8.0
    DESCRIPTION "POS-based cryptocurrency that rewards BOINC computation"
    HOMEPAGE_URL "https://gridcoin.us"
    LANGUAGES C CXX
)

set(CLIENT_VERSION_IS_RELEASE "true")
set(COPYRIGHT_YEAR "2024")
set(COPYRIGHT_HOLDERS_FINAL "The Gridcoin developers")


# Toolchain configuration
# =======================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(MSVC)
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        message(FATAL_ERROR "It's not yet possible to build Gridcoin with MSVC")
    endif()
    add_compile_options(/U NDEBUG)
else()
    add_compile_options(-UNDEBUG)
endif()


# Load CMake modules
# ==================

include(CheckCXXSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckPIESupported)
include(CheckSymbolExists)

include(CheckSSE)
include(CheckStrerrorR)
include(HunterGate)
include(VersionFromGit)


# Define options
# ==============

# Build configuration
option(ENABLE_DAEMON    "Enable daemon" ON)
option(ENABLE_GUI       "Enable Qt-based GUI" OFF)
option(ENABLE_DOCS      "Build Doxygen documentation" OFF)
option(ENABLE_TESTS     "Build tests" OFF)
option(LUPDATE          "Update translation files" OFF)
option(STATIC_LIBS      "Prefer static variants of system libraries" ${WIN32})
option(STATIC_RUNTIME   "Link runtime statically" ${WIN32})

# CPU-dependent options
option(ENABLE_SSE41     "Build code that uses SSE4.1 intrinsics" ${HAS_SSE41})
option(ENABLE_AVX2      "Build code that uses AVX2 intrinsics" ${HAS_AVX2})
option(ENABLE_X86_SHANI "Build code that uses x86 SHA-NI intrinsics" ${HAS_X86_SHANI})
option(ENABLE_ARM_SHANI "Build code that uses ARM SHA-NI intrinsics" ${HAS_ARM_SHANI})
option(USE_ASM          "Enable assembly routines" ON)

# Optional functionality
option(ENABLE_PIE       "Build position-independent executables" OFF)
option(ENABLE_QRENCODE  "Enable generation of QR Codes for receiving payments" OFF)
option(ENABLE_UPNP      "Enable UPnP port mapping support" OFF)
option(DEFAULT_UPNP     "Turn UPnP on startup" OFF)
option(USE_DBUS         "Enable DBus support" OFF)

# Bundled packages
option(SYSTEM_BDB	    "Find system installation of Berkeley DB CXX 5.3" OFF)
option(SYSTEM_LEVELDB   "Find system installation of leveldb" OFF)
option(SYSTEM_SECP256K1 "Find system installation of libsecp256k1 with pkg-config" OFF)
option(SYSTEM_UNIVALUE  "Find system installation of Univalue with pkg-config" OFF)
option(SYSTEM_XXD       "Find system xxd binary" OFF)

# Hunter packages
option(BUNDLED_BOOST    "Use the bundled version of Boost" ${HUNTER_ENABLED})
option(BUNDLED_CURL     "Use the bundled version of cURL" ${HUNTER_ENABLED})
option(BUNDLED_LIBZIP   "Use the bundled version of libzip" ${HUNTER_ENABLED})
option(BUNDLED_OPENSSL  "Use the bundled version of OpenSSL" ${HUNTER_ENABLED})
option(BUNDLED_QT       "Use the bundled version of Qt" ${HUNTER_ENABLED})


# Handle dependencies
# ===================

set(QT5_MINIMUM_VERSION 5.9.5)
set(QT5_COMPONENTS Concurrent Core Gui LinguistTools Network Widgets)
set(QT5_HUNTER_COMPONENTS qtbase qttools)
if(USE_DBUS)
    list(APPEND QT5_COMPONENTS DBus)
endif()
if(ENABLE_TESTS)
    list(APPEND QT5_COMPONENTS Test)
endif()

set(BOOST_MINIMUM_VERSION 1.63.0)
set(BOOST_COMPONENTS filesystem iostreams thread)
set(BOOST_HUNTER_COMPONENTS ${BOOST_COMPONENTS})
if(ENABLE_TESTS)
    list(APPEND BOOST_COMPONENTS unit_test_framework)
    list(APPEND BOOST_HUNTER_COMPONENTS test)
endif()

find_package(Atomics REQUIRED)
find_package(Threads REQUIRED)

if(SYSTEM_BDB)
    find_package(BerkeleyDB 5.3...<5.4 COMPONENTS CXX REQUIRED)
else()
    find_program(SH_EXE NAMES sh bash REQUIRED)
    find_program(MAKE_EXE NAMES gmake nmake make REQUIRED)
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

if(BUNDLED_BOOST)
    hunter_add_package(Boost COMPONENTS ${BOOST_HUNTER_COMPONENTS})
endif()
find_package(Boost ${BOOST_MINIMUM_VERSION} COMPONENTS ${BOOST_COMPONENTS} CONFIG REQUIRED)

if(BUNDLED_OPENSSL)
    hunter_add_package(OpenSSL)
endif()
find_package(OpenSSL REQUIRED)

if(BUNDLED_CURL)
    hunter_add_package(CURL)
    find_package(CURL CONFIG REQUIRED)
else()
    find_package(CURL REQUIRED)
endif()

if(BUNDLED_LIBZIP)
    hunter_add_package(libzip)
endif()
find_package(libzip CONFIG REQUIRED)

if(USE_ASM)
    enable_language(ASM)
endif()

if(ENABLE_GUI)
    if(BUNDLED_QT)
        hunter_add_package(Qt COMPONENTS ${QT5_HUNTER_COMPONENTS})
    endif()
    find_package(Qt5 ${QT5_MINIMUM_VERSION} COMPONENTS ${QT5_COMPONENTS} REQUIRED)

    if(ENABLE_QRENCODE)
        pkg_check_modules(QRENCODE REQUIRED IMPORTED_TARGET libqrencode)
    endif()

    # Compatibility macros
    if(Qt5Core_VERSION VERSION_LESS 5.15.0)
        macro(qt_create_translation)
            qt5_create_translation(${ARGN})
        endmacro()

        macro(qt_add_translation)
            qt5_add_translation(${ARGN})
        endmacro()
    endif()
endif()

if(ENABLE_UPNP)
    pkg_check_modules(MINIUPNPC REQUIRED IMPORTED_TARGET miniupnpc>=1.9)
endif()

if(ENABLE_TESTS)
    enable_testing()

    if(SYSTEM_XXD)
        find_program(XXD xxd REQUIRED)
    endif()
endif()

if(UNIX)
    find_package(Rt REQUIRED)
endif()

if(WIN32)
    enable_language(RC)
    find_program(MAKENSIS makensis)
elseif(APPLE)
    enable_language(OBJCXX)
endif()


# Run probes
# ==========

if(ENABLE_PIE)
    check_pie_supported()
    if(NOT CMAKE_CXX_LINK_PIE_SUPPORTED)
        message(FATAL_ERROR "PIE is not supported by the current linker")
    endif()
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ${ENABLE_PIE})

# Set compiler flags
if(APPLE)
    add_compile_options(-Wno-error=deprecated-declarations)
    add_compile_options(-Wno-error=thread-safety-analysis)
    add_compile_options(-Wno-error=thread-safety-reference)
endif()

if(STATIC_LIBS)
    set(CMAKE_LINK_SEARCH_START_STATIC ON)
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()

if(STATIC_RUNTIME)
    if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)\$")
        list(APPEND RUNTIME_LIBS -static-libgcc -static-libstdc++)
    elseif(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

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
