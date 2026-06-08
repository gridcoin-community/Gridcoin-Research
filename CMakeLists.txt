# CMake is the official build system for Gridcoin.

cmake_minimum_required(VERSION 3.18)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/build-aux/cmake")
set(CPACK_MODULE_PATH "${CPACK_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/build-aux/cpack")


# Hunter Package Manager
# ======================

# Windows doesn't yet have a package manager that can be used for managing
# dependencies, so we use Hunter on it.
option(HUNTER_ENABLED "Enable Hunter package manager" OFF)
include(HunterGate)
HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.25.8.tar.gz"
    SHA1 "26c79d587883ec910bce168e25f6ac4595f97033"
    FILEPATH "${CMAKE_CURRENT_SOURCE_DIR}/build-aux/cmake/Hunter/config.cmake"
)


# Project configuration
# =====================

project("Gridcoin"
    VERSION 5.5.1.1
    DESCRIPTION "POS-based cryptocurrency that rewards BOINC computation"
    HOMEPAGE_URL "https://gridcoin.us"
    LANGUAGES C CXX
)

set(CLIENT_VERSION_IS_RELEASE "false")
set(COPYRIGHT_YEAR "2026")
set(COPYRIGHT_HOLDERS_FINAL "The Gridcoin developers")


# Toolchain configuration
# =======================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_compile_definitions(HAVE_CMAKE)

if(MSVC)
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        message(FATAL_ERROR "It's not yet possible to build Gridcoin with MSVC")
    endif()
    add_compile_options(/U NDEBUG)
else()
    add_compile_options(-UNDEBUG)
endif()

# Define DEBUG for Debug builds to match the old Autotools --enable-debug behavior.
# Code in init.cpp and span.h uses #ifdef DEBUG to select debug-specific paths.
# Uses a generator expression so it works with multi-config generators too.
add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)


# Load CMake modules
# ==================

include(CheckCXXSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckPIESupported)
include(CheckSymbolExists)

# SIMD checks are compile-only, so they are safe for cross-compilation.
# We run them unconditionally. The compiler flags will determine success/fail.
include(CheckSSE41)
include(CheckAVX2)
include(CheckSHANI)
include(CheckARMSHANI)

# Runtime checks must still be skipped during cross-compilation
if(NOT CMAKE_CROSSCOMPILING)
    include(CheckStrerrorR)
else()
    message(STATUS "Cross-compiling: skipping CheckStrerrorR probe.")
endif()

include(FindPkgConfig)
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

# Scrypt assembly uses GNU assembler syntax incompatible with Apple's
# LLVM assembler, so default to OFF on Apple and ON elsewhere.
if(NOT DEFINED USE_ASM_SCRYPT)
    if(APPLE)
        set(USE_ASM_SCRYPT OFF)
    else()
        set(USE_ASM_SCRYPT ${USE_ASM})
    endif()
endif()
option(USE_ASM_SCRYPT   "Enable scrypt assembly routines (requires GNU assembler)" ${USE_ASM_SCRYPT})

# Optional functionality
option(ENABLE_PIE       "Build position-independent executables" OFF)
option(ENABLE_DEBUG_LOCKORDER "Enable run-time lock-order checking (DEBUG_LOCKORDER)" OFF)
option(WERROR_THREAD_SAFETY "Promote Clang thread-safety diagnostics to build errors" OFF)
option(ENABLE_QRENCODE  "Enable generation of QR Codes for receiving payments" OFF)
option(ENABLE_UPNP      "Enable UPnP port mapping support" OFF)
option(DEFAULT_UPNP     "Turn UPnP on startup" OFF)
option(USE_DBUS         "Enable DBus support" OFF)
option(USE_QT6          "Use Qt 6 instead of Qt 5" OFF)

# Bundled packages
option(SYSTEM_BDB       "Find system installation of Berkeley DB CXX 5.3" OFF)
option(SYSTEM_LEVELDB   "Find system installation of leveldb" OFF)
option(SYSTEM_SECP256K1 "Find system installation of libsecp256k1 with pkg-config" OFF)
option(SYSTEM_UNIVALUE  "Find system installation of Univalue with pkg-config" OFF)
option(SYSTEM_XXD       "Find system xxd binary" OFF)

# Hunter packages
option(BUNDLED_BOOST    "Use the bundled version of Boost" ${HUNTER_ENABLED})
option(BUNDLED_CURL     "Use the bundled version of cURL" ${HUNTER_ENABLED})
option(BUNDLED_LIBZIP   "Use the bundled version of libzip" ${HUNTER_ENABLED})
option(BUNDLED_OPENSSL  "Use the bundled version of OpenSSL" ${HUNTER_ENABLED})
option(BUNDLED_QT5      "Use the bundled version of Qt 5" ${HUNTER_ENABLED})


# Handle dependencies
# ===================

set(MINIUPNPC_USE_STATIC_LIBS ${STATIC_LIBS})

set(QT5_MINIMUM_VERSION 5.9.5)
set(QT6_MINIMUM_VERSION 6.2.0)
set(QT_COMPONENTS Core Concurrent Gui LinguistTools Network Widgets Svg)
set(QT_HUNTER_COMPONENTS qtbase qttools)
if(USE_QT6)
    list(APPEND QT_COMPONENTS Core5Compat)
endif()
if(USE_DBUS)
    list(APPEND QT_COMPONENTS DBus)
endif()
if(ENABLE_TESTS)
    list(APPEND QT_COMPONENTS Test)
endif()

set(BOOST_MINIMUM_VERSION 1.63.0)
set(BOOST_COMPONENTS filesystem iostreams thread serialization date_time)
set(BOOST_HUNTER_COMPONENTS system atomic regex ${BOOST_COMPONENTS})
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

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.30)
    # Allow to find old Boost (pre 1.70) with new CMake (3.30 and later).
    cmake_policy(SET CMP0167 OLD)
endif()

find_package(Boost ${BOOST_MINIMUM_VERSION} REQUIRED)

if(Boost_VERSION VERSION_LESS 1.69.0)
    list(APPEND BOOST_COMPONENTS system)
endif()

if(Boost_VERSION VERSION_LESS 1.70.0)
    find_package(Boost ${BOOST_MINIMUM_VERSION} COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
else()
    # Better upstream-provided CMake config is available.
    find_package(Boost ${BOOST_MINIMUM_VERSION} COMPONENTS ${BOOST_COMPONENTS} CONFIG REQUIRED)
endif()

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

find_package(ZLIB REQUIRED)

if(USE_ASM)
    enable_language(ASM)
endif()

if(ENABLE_GUI)
    set(QT_MACOS_DISABLE_DARK_MODE False)
    if(USE_QT6)
        find_package(Qt6 ${QT6_MINIMUM_VERSION} COMPONENTS ${QT_COMPONENTS} REQUIRED)
        set(QT Qt6)
    else()
        if(BUNDLED_QT5)
            hunter_add_package(Qt COMPONENTS ${QT_HUNTER_COMPONENTS})
        endif()
        find_package(Qt5 ${QT5_MINIMUM_VERSION} COMPONENTS ${QT_COMPONENTS} REQUIRED)
        set(QT Qt5)

        if(Qt5Core_VERSION VERSION_LESS 5.12.0)
            set(QT_MACOS_DISABLE_DARK_MODE True)
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

    if(ENABLE_QRENCODE)
        pkg_check_modules(QRENCODE REQUIRED IMPORTED_TARGET libqrencode)
    endif()
endif()

if(ENABLE_UPNP)
    pkg_check_modules(MINIUPNPC IMPORTED_TARGET miniupnpc>=1.9)
    if(MINIUPNPC_FOUND)
        set(LIBMINIUPNPC PkgConfig::MINIUPNPC)
    else()
        find_package(miniupnpc CONFIG REQUIRED)
        set(LIBMINIUPNPC miniupnpc::miniupnpc)
    endif()
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
    # MAC_OSX gates macOS-specific code paths inherited from Bitcoin (data
    # directory under ~/Library/Application Support, F_FULLFSYNC, getentropy,
    # pthread thread names). The Autotools build defined it via configure.ac;
    # the move to CMake dropped it, silently disabling those paths on macOS.
    add_compile_definitions(MAC_OSX)
endif()

# Enable Clang Thread Safety Analysis where supported (issue #2869). The
# analyzer ships in upstream Clang and Apple Clang; gcc has no equivalent
# attribute set. Detect rather than gating on CMAKE_CXX_COMPILER_ID so any
# clang-compatible compiler picks it up (e.g. clang-cl on Windows).
#
# By default thread-safety diagnostics are warning-only (the -Wno-error=
# flags below): a stale or missing annotation does not break a normal build.
# When WERROR_THREAD_SAFETY is set they are promoted to hard errors instead.
# The "Thread Safety (Clang)" job in .github/workflows/cmake_quality.yml
# builds with WERROR_THREAD_SAFETY=ON, so an annotation regression fails CI;
# a local build can opt in the same way. See doc/developer-notes.md, section
# "Clang thread-safety analysis".
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wthread-safety HAVE_CLANG_THREAD_SAFETY)
if(HAVE_CLANG_THREAD_SAFETY)
    add_compile_options(-Wthread-safety)
    if(WERROR_THREAD_SAFETY)
        add_compile_options(-Werror=thread-safety-analysis)
        add_compile_options(-Werror=thread-safety-reference)
    else()
        add_compile_options(-Wno-error=thread-safety-analysis)
        add_compile_options(-Wno-error=thread-safety-reference)
    endif()
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
if(CMAKE_CROSSCOMPILING AND WIN32)
    message(STATUS "Cross-compiling for Windows: forcing POSIX headers to 'not found'")
    set(HAVE_BYTESWAP_H FALSE CACHE INTERNAL "Manually set to false for Windows cross-compile")
    set(HAVE_ENDIAN_H FALSE CACHE INTERNAL "Manually set to false for Windows cross-compile")
    set(HAVE_SYS_ENDIAN_H FALSE CACHE INTERNAL "Manually set to false for Windows cross-compile")
    set(HAVE_SYS_PRCTL_H FALSE CACHE INTERNAL "Manually set to false for Windows cross-compile")
else()
    check_include_file("byteswap.h" HAVE_BYTESWAP_H)
    check_include_file("endian.h" HAVE_ENDIAN_H)
    check_include_file("sys/endian.h" HAVE_SYS_ENDIAN_H)
    check_include_file("sys/prctl.h" HAVE_SYS_PRCTL_H)
endif()

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
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(pipe2 "unistd.h" HAVE_DECL_PIPE2)
list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
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

check_symbol_exists(getrandom "sys/random.h" HAVE_GETRANDOM)
check_symbol_exists(getentropy "sys/random.h" HAVE_GETENTROPY_RAND)
check_symbol_exists(sysctl "sys/sysctl.h" "sys/types.h" HAVE_SYSCTL)
check_symbol_exists(KERN_ARND "sys/sysctl.h" "sys/types.h" HAVE_SYSCTL_ARND)

check_symbol_exists(O_CLOEXEC "fcntl.h" HAVE_O_CLOEXEC)
check_symbol_exists(getauxval "sys/auxv.h" HAVE_STRONG_GETAUXVAL)

# Descend into subdirectories
# ===========================

add_subdirectory(src)
if(ENABLE_DOCS)
    add_subdirectory(doc)
endif()

# CPack packaging
# ===============

# Common metadata
set(CPACK_PACKAGE_VERSION_TWEAK ${PROJECT_VERSION_TWEAK})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "GridcoinResearch")
set(CPACK_PACKAGE_VENDOR "The Gridcoin Community")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Gridcoin Research Wallet")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://gridcoin.us")
set(CPACK_PACKAGE_CONTACT "Gridcoin Developers <dev@gridcoin.us>")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_COPYRIGHT_STRING "Copyright (C) 2014-${COPYRIGHT_YEAR} ${COPYRIGHT_HOLDERS_FINAL}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_CHANGELOG "${CMAKE_SOURCE_DIR}/CHANGELOG.md")
set(CPACK_ICONDIR "${CMAKE_SOURCE_DIR}/share/pixmaps")

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(CPACK_WINDOWS_BITS 32)
else()
    set(CPACK_WINDOWS_BITS 64)
endif()

if(CMAKE_C_COMPILER_TARGET)
    # Cross/depends builds: e.g. x86_64-pc-linux-gnu, x86_64-w64-mingw32
    set(CPACK_SYSTEM_NAME "${CMAKE_C_COMPILER_TARGET}")
elseif(CMAKE_SYSTEM_PROCESSOR)
    # Native builds: e.g. x86_64, aarch64
    set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_PROCESSOR}")
endif()

# CPack constructs CPACK_PACKAGE_VERSION from MAJOR.MINOR.PATCH, excluding
# TWEAK. Override it to use the full quad version (e.g. 5.4.9.10).
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")

set(CPACK_NSIS_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_SOURCE_PACKAGE_FILE_NAME "gridcoinresearch-${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_SOURCE_PACKAGE_FILE_NAME}-${CPACK_SYSTEM_NAME}")
set(CPACK_DMG_VOLUME_NAME "Gridcoin")

# Strip the binaries to make the tarball smaller
set(CPACK_STRIP_FILES ON)

# File included at cpack time, once per generator after setting CPACK_GENERATOR
# to the actual generator being used; allows per-generator setting of CPACK_*
# variables at cpack time.
configure_file(
    "${CMAKE_SOURCE_DIR}/build-aux/cpack/CPackOptions.cmake.in"
    "${CMAKE_BINARY_DIR}/CPackOptions.cmake"
    @ONLY
)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackOptions.cmake")

include(CPack)
