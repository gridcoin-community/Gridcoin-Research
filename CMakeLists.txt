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
    VERSION 5.5.1.7
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
option(ENABLE_PIE       "Build position-independent executables. OFF genuinely disables PIE, it does not defer to the toolchain default" OFF)
option(ENABLE_HARDENING "Apply exploit-mitigation compiler and linker flags" ON)
option(ENABLE_DEBUG_LOCKORDER "Enable run-time lock-order checking (DEBUG_LOCKORDER)" OFF)
option(WERROR_THREAD_SAFETY "Promote Clang thread-safety diagnostics to build errors" OFF)
option(ENABLE_QRENCODE  "Enable generation of QR Codes for receiving payments" OFF)
option(ENABLE_UPNP      "Enable UPnP port mapping support" ON)
option(DEFAULT_UPNP     "Turn UPnP on startup" OFF)
option(USE_DBUS         "Enable DBus support" OFF)
option(USE_QT6          "Use Qt 6 instead of Qt 5" OFF)

# Multiprocess (Phase 2 GUI/node IPC split, RFC #2937). OFF = the monolithic
# build with direct in-process calls (current behavior); ON = build against
# Cap'n Proto + libmultiprocess so the interfaces:: boundaries can be driven
# over IPC. This flag only wires the toolchain (dependency detection + codegen
# tooling); it does not by itself change any runtime behavior yet.
option(ENABLE_MULTIPROCESS "Build with Cap'n Proto + libmultiprocess for the multiprocess (IPC) build" OFF)

# Build against an external libmultiprocess (system / depends install) instead of
# the vendored in-tree subtree (src/ipc/libmultiprocess). OFF (the default) builds
# the runtime from the subtree, matching Bitcoin Core; ON is mainly useful for
# developing libmultiprocess itself against a separate checkout.
option(WITH_EXTERNAL_LIBMULTIPROCESS "Use an external libmultiprocess instead of the vendored subtree" OFF)

# Bundled packages
option(SYSTEM_BDB       "Find system installation of Berkeley DB CXX 5.3" OFF)
option(SYSTEM_LEVELDB   "Find system installation of leveldb" OFF)
option(SYSTEM_SECP256K1 "Find system installation of libsecp256k1 with pkg-config" OFF)
option(SYSTEM_UNIVALUE  "Find system installation of Univalue with pkg-config" OFF)
option(SYSTEM_XXD       "Find system xxd binary" OFF)

# Hunter packages
option(BUNDLED_BOOST    "Use the bundled version of Boost" ${HUNTER_ENABLED})
option(BUNDLED_CURL     "Use the bundled version of cURL" ${HUNTER_ENABLED})
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

# 1.66 is the oldest release this source can actually compile against, and has
# been for some time: rpc/server.cpp uses boost::asio::ip::make_address_v6()
# with no version guard, and that entered Boost in 1.66 along with the asio
# rewrite. The 1.63 declared here was simply never revisited when that landed,
# so it promised a floor no build has met in a while.
#
# 1.66 is the floor the source implies, not a version anyone exercises: CI
# builds against the distribution Boost and depends pins 1.89. The io_context /
# io_service shim in rpc/protocol.h still guards at 1.70, so 1.66-1.69 remains
# intended to work.
set(BOOST_MINIMUM_VERSION 1.66.0)
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

# CMake's FindBoost defaults Boost_USE_DEBUG_RUNTIME to TRUE when the caller has not
# set it. That is an MSVC-only concept -- it selects the /MDd runtime -- but Boost's own
# CMake config files honour it on every platform: each variant file begins with
#
#     if(Boost_USE_DEBUG_RUNTIME)
#       _BOOST_SKIPPED("libboost_foo.so" "release runtime, ...")
#
# so on a distribution that ships only release-runtime variants EVERY variant is rejected
# and Boost is reported as not found ("No suitable build variant has been found").
# FreeBSD's boost packages are built that way.
#
# This only bites when FindBoost actually runs its module search: FindBoost first delegates
# to find_package(Boost NO_MODULE) and returns early if a BoostConfig.cmake is found, in
# which case the default is never assigned. So the failure appears on systems whose Boost
# lacks the CMake config package, and not on those that have it -- which is why it is not
# reproducible everywhere.
#
# Set it OFF up front rather than unsetting it afterwards: FindBoost only assigns the
# default under if(NOT DEFINED ...), so a value defined here is preserved, it covers the
# pre-1.70 module-mode branch below as well, and it does not discard a value a user or
# toolchain file set deliberately.
if(NOT DEFINED Boost_USE_DEBUG_RUNTIME)
    set(Boost_USE_DEBUG_RUNTIME OFF)
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
        find_package(miniupnpc CONFIG QUIET)

        if(miniupnpc_FOUND)
            set(LIBMINIUPNPC miniupnpc::miniupnpc)
        else()
            # ENABLE_UPNP defaults ON so the GUI setting is always operative, but
            # the library is optional and was not previously needed for a plain
            # configure. Requiring it here would turn "cmake -B build" into a hard
            # failure on any machine without it. Build without UPnP instead, and
            # say so.
            # No "pass -DENABLE_UPNP=ON to make this fatal" advice here: ENABLE_UPNP
            # already defaults to ON, and option() caches, so this branch cannot tell
            # an explicit -DENABLE_UPNP=ON from the default. An earlier revision tried
            # to and became fatal on every RE-configure once the value was cached.
            # Offering a switch that does nothing is worse than not offering one.
            message(WARNING "miniupnpc not found: building without UPnP support.")
            set(ENABLE_UPNP OFF)
        endif()
    endif()
endif()

if(ENABLE_MULTIPROCESS)
    # The IPC transport is pathname AF_UNIX sockets (src/ipc/). AF_UNIX is not
    # POSIX-only: Windows 10 1803+ provides it via <afunix.h> with the same
    # sockaddr_un layout, so the same transport builds on both platforms. The
    # Windows port swaps the POSIX socket/close-on-exec/permission primitives for
    # their winsock equivalents (WSASocketW / handle-inherit flags / NTFS ACLs)
    # in src/ipc/process.cpp and fences the unused fork/spawn paths in the
    # libmultiprocess subtree; both platforms are driven by this one switch.

    # Cap'n Proto provides the serialization runtime and the capnp / capnpc-c++
    # code generators; libmultiprocess provides the proxy runtime and the mpgen
    # generator that turns an interfaces .capnp schema into client/server proxies.
    #
    # Cap'n Proto is always an external dependency (system / /usr/local, or the
    # depends 'capnp' package). In a depends build the toolchain file
    # (depends/toolchain.cmake.in) pre-seeds MPGEN_EXECUTABLE / CAPNP_EXECUTABLE /
    # CAPNPC_CXX_EXECUTABLE to the tools it built and points CMAKE_PREFIX_PATH at
    # the staged libraries, so find_package resolves there.
    find_package(CapnProto CONFIG REQUIRED)

    if(WITH_EXTERNAL_LIBMULTIPROCESS)
        # Developer path: link an external libmultiprocess and locate its mpgen.
        find_package(Libmultiprocess CONFIG REQUIRED)
        if(NOT MPGEN_EXECUTABLE)
            find_program(MPGEN_EXECUTABLE NAMES mpgen REQUIRED)
        endif()
        message(STATUS "Multiprocess (IPC) build: ENABLED (external libmultiprocess)")
        message(STATUS "  mpgen:       ${MPGEN_EXECUTABLE}")
    else()
        # Default path: build the vendored subtree in-tree (see src/CMakeLists.txt,
        # add_libmultiprocess()). It produces the 'multiprocess' runtime library and
        # the 'mpgen' code generator as build targets.
        message(STATUS "Multiprocess (IPC) build: ENABLED (vendored subtree)")
    endif()
    message(STATUS "  Cap'n Proto: ${CapnProto_VERSION}")
    # Compile definition so the -multiprocess seam code (in gridcoinresearchd and
    # the GUI) is gated: OFF builds neither reference nor link gridcoin_ipc.
    add_compile_definitions(ENABLE_MULTIPROCESS)
else()
    message(STATUS "Multiprocess (IPC) build: disabled (monolithic, direct calls)")
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

# check_pie_supported() is what activates policy CMP0083's handling of
# POSITION_INDEPENDENT_CODE on executables. Probing only when ENABLE_PIE was ON
# left the property silently ignored in BOTH directions whenever it was OFF:
# CMake emitted neither -pie nor -no-pie, so the option could turn PIE on but
# never off, and OFF actually meant "whatever this toolchain happens to default
# to". Probe unconditionally so the setting is honoured either way -- OFF then
# genuinely emits the no-PIE link flags rather than nothing.
check_pie_supported()

if(ENABLE_PIE AND NOT CMAKE_CXX_LINK_PIE_SUPPORTED)
    message(FATAL_ERROR "PIE is not supported by the current linker")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ${ENABLE_PIE})

# Exploit mitigations
# ===================
#
# Restores the set the Autotools build applied. There --enable-hardening was the
# DEFAULT (the spelling was --disable-hardening, i.e. opt out) and configure.ac
# fed HARDENED_CXXFLAGS/CPPFLAGS/LDFLAGS into AM_* so every target inherited
# them. The CMake port carried none of it across, so nothing built here has had a
# stack canary, FORTIFY, full RELRO or CET since the migration. ON by default
# restores that posture rather than inventing a new one; distributions that
# manage their own hardening can pass -DENABLE_HARDENING=OFF.
#
# Every flag is probed before use, so a toolchain that outright rejects one simply
# does not get it -- that is what keeps the ELF-only linker flags off Mach-O and
# -fcf-protection off arm64, where they are not valid options.
#
# Probing alone is not sufficient, though: it answers "does the driver accept
# this?", not "does this target honour the code it generates?". Flags that are
# accepted everywhere but only meaningful on ELF are gated on the platform below.
include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)

if(ENABLE_HARDENING)
    # _FORTIFY_SOURCE needs optimisation; without it GCC warns and the checks do
    # nothing. Debug is excluded via a generator expression. Sanitizer builds are
    # excluded too: they are built RelWithDebInfo here, so the config test alone
    # would not catch them, and the instrumentation does not combine well with
    # FORTIFY's interception of the same calls.
    string(FIND "${CMAKE_CXX_FLAGS}" "-fsanitize" _hardening_sanitizer_pos)
    if(_hardening_sanitizer_pos EQUAL -1)
        # -U first: some toolchains predefine it, and redefining warns.
        # Guard on there being an optimised config at all, not merely on the
        # config not being named Debug: with no CMAKE_BUILD_TYPE the compiler runs
        # at -O0, and FORTIFY then warns on every translation unit while checking
        # nothing. doc/build.md describes that configuration, so it is reachable.
        if(CMAKE_BUILD_TYPE OR CMAKE_CONFIGURATION_TYPES)
            add_compile_options(
                "$<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE>"
                "$<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>")
        else()
            message(STATUS "Hardening: _FORTIFY_SOURCE skipped (no build type, so no optimisation)")
        endif()
    else()
        message(STATUS "Hardening: _FORTIFY_SOURCE skipped (sanitizer build)")
    endif()

    # Stack canaries are supported by every toolchain we ship, on every target.
    set(hardening_flags -fstack-protector-all -Wstack-protector)

    # The rest are ELF/AAPCS mitigations, and they are gated on the platform
    # rather than on the probe alone. check_cxx_compiler_flag proves only that the
    # driver ACCEPTS a flag -- never that the target's runtime honours the code it
    # generates. Clang accepts all three on targets where they do nothing
    # (-mbranch-protection is inert on x86) or are actively wrong, so probing
    # cannot tell the two apart and an earlier revision of this block assumed a
    # symmetry that the build logs do not show.
    #
    # On Apple Silicon -mbranch-protection=standard emits pointer-authentication
    # sign/authenticate pairs around returns. PAC on Darwin is an ABI-level opt-in
    # (arm64e), not a per-flag one: on plain arm64 the signed return addresses
    # defeat libunwind, so a throw never reaches a handler that is lexically right
    # above it. That turned the in-scope catch in ThreadSocketHandler into a
    # "terminating due to uncaught exception" abort and hung the macOS ARM64
    # functional tests, while macOS Intel -- where the same flag is a no-op --
    # passed in three minutes. Keep all three off Mach-O.
    # -fcf-protection=full stays on every platform. It is meaningful on macOS Intel
    # -- the Mach-O x86_64 CONTROL_FLOW check reads endbr64 at the entrypoint, and
    # that check passes today -- and the probe rejects it outright on arm64, so it
    # lands only where it does something.
    list(APPEND hardening_flags -fcf-protection=full)

    # Two flags are withheld from Mach-O:
    #
    #   -mbranch-protection=standard emits pointer-authentication sign/authenticate
    #   pairs around returns on AArch64. PAC on Darwin is an ABI-level opt-in
    #   (arm64e), not a per-flag one, and on plain arm64 the signed return addresses
    #   stop a throw from reaching its handler -- an exception raised directly
    #   beneath an in-scope catch aborted as "uncaught", and the macOS ARM64
    #   functional tests hung. Removing it turned that job green.
    #
    #   -fstack-clash-protection is accepted by the driver and then reported as
    #   "argument unused during compilation" on every translation unit, so it buys
    #   nothing on Darwin and costs several hundred warnings.
    #
    # security-check.py drops the Mach-O arm64 BRANCH_PROTECTION check to match;
    # the two must be changed together or the artifact check contradicts the build.
    if(NOT APPLE)
        list(APPEND hardening_flags -fstack-clash-protection -mbranch-protection=standard)
    else()
        message(STATUS "Hardening: branch-protection and stack-clash skipped (Mach-O target)")
    endif()

    # Make the probes reject a flag the driver accepts but then does not implement.
    #
    # check_cxx_compiler_flag only proves the driver did not reject the spelling. Clang
    # will accept a flag it has no implementation for on the current target and then
    # report "argument unused during compilation: '-fstack-clash-protection'" on EVERY
    # translation unit: the flag is requested, the mitigation is absent, and the build
    # gains several hundred warnings saying so. OpenBSD/amd64 does exactly this with
    # -fstack-clash-protection (316 warnings in a full build), and Darwin does the same.
    #
    # Promoting that diagnostic to an error for the duration of the probes turns
    # "accepted but ignored" into a failed check, so the flag is simply not added. GCC
    # has no such warning and errors on the option, so probe for it first -- without the
    # guard, setting it unconditionally would make every hardening probe below fail on
    # GCC and silently disable hardening on Linux.
    #
    # This does not catch a flag that is accepted AND honoured but semantically wrong for
    # the target -- see -mbranch-protection on Mach-O above, which generates real code
    # that breaks unwinding. No probe can see that; it stays platform-gated.
    check_cxx_compiler_flag("-Werror=unused-command-line-argument" HARDENING_HAVE_WERROR_UNUSED_ARG)
    set(_hardening_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
    if(HARDENING_HAVE_WERROR_UNUSED_ARG)
        string(APPEND CMAKE_REQUIRED_FLAGS " -Werror=unused-command-line-argument")
    endif()

    foreach(flag ${hardening_flags})
        string(MAKE_C_IDENTIFIER "HARDENING_CXX${flag}" flag_var)
        check_cxx_compiler_flag("${flag}" ${flag_var})
        if(${flag_var})
            add_compile_options("${flag}")
        else()
            message(STATUS "Hardening: ${flag} not applied (compiler accepts no working "
                           "implementation for this target)")
        endif()
    endforeach()

    set(CMAKE_REQUIRED_FLAGS "${_hardening_saved_required_flags}")
    unset(_hardening_saved_required_flags)

    # separate-code keeps executable and non-executable content on distinct pages,
    # which shrinks what a ROP search has to work with. GNU ld enables it by default
    # on x86 since binutils 2.31, so x86 had it accidentally; the aarch64 cross
    # linker does not, which is how the arm64 job failed this check while every
    # x86 job passed. Ask for it rather than inheriting it.
    foreach(flag -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,-z,separate-code)
        string(MAKE_C_IDENTIFIER "HARDENING_LD${flag}" flag_var)
        check_linker_flag(CXX "${flag}" ${flag_var})
        if(${flag_var})
            add_link_options("${flag}")
        endif()
    endforeach()
endif()

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

# Python functional test suite (test/functional). Phase 4 — refs #2932.
# ====================================================================
# Generates test/config.ini from config.ini.in, then registers:
#   * a CTest case `functional_tests` (run via `ctest -R functional_tests`)
#   * a convenience target `check-functional` (run via `cmake --build . --target check-functional`)
# both of which drive test/functional/test_runner.py.
if(ENABLE_TESTS AND ENABLE_DAEMON)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        set(PACKAGE_NAME "${PROJECT_NAME}")
        set(PACKAGE_BUGREPORT "https://github.com/gridcoin-community/Gridcoin-Research/issues")
        set(ENABLE_GRIDCOIND_CONF "true")
        set(ENABLE_CLI_CONF "true")      # no separate gridcoin-cli, but the daemon
                                         # doubles as the RPC client (GRIDCOINCLI below
                                         # points at it), so CLI tests can run.
        set(ENABLE_WALLET_CONF "true")   # Gridcoin always builds the BDB wallet
        set(ENABLE_ZMQ_CONF "false")     # Gridcoin has no ZMQ support

        configure_file(
            "${CMAKE_SOURCE_DIR}/test/config.ini.in"
            "${CMAKE_BINARY_DIR}/test/config.ini"
            @ONLY
        )

        # gridcoinresearchd is emitted to ${CMAKE_BINARY_DIR}/bin (see
        # src/CMakeLists.txt RUNTIME_OUTPUT_DIRECTORY), not the src/ path the
        # ported framework defaults to, so point it there via the framework's
        # supported GRIDCOIND/GRIDCOINCLI overrides. (The daemon doubles as CLI.)
        set(_functional_env
            "GRIDCOIND=${CMAKE_BINARY_DIR}/bin/gridcoinresearchd${CMAKE_EXECUTABLE_SUFFIX}"
            "GRIDCOINCLI=${CMAKE_BINARY_DIR}/bin/gridcoinresearchd${CMAKE_EXECUTABLE_SUFFIX}"
        )

        # Only register the CTest case for native builds: the functional suite
        # spawns and drives a real gridcoinresearchd, which cannot run under a
        # cross-compile's emulator (e.g. the Windows mingw build's `ctest` runs
        # under wine on a Linux host). Cross builds still get config.ini and the
        # check-functional target, just not the auto-run ctest case.
        if(NOT CMAKE_CROSSCOMPILING)
            add_test(
                NAME functional_tests
                COMMAND ${Python3_EXECUTABLE}
                        "${CMAKE_SOURCE_DIR}/test/functional/test_runner.py"
                        "--configfile=${CMAKE_BINARY_DIR}/test/config.ini"
                        --ci --quiet --combinedlogslen=4000 --attempts=3
            )
            set_tests_properties(functional_tests PROPERTIES
                ENVIRONMENT "${_functional_env}"
                RUN_SERIAL TRUE   # tests spin up daemons; don't overlap with other ctest cases
                TIMEOUT 1800
            )
        endif()

        add_custom_target(check-functional
            COMMAND ${CMAKE_COMMAND} -E env ${_functional_env}
                    ${Python3_EXECUTABLE}
                    "${CMAKE_SOURCE_DIR}/test/functional/test_runner.py"
                    "--configfile=${CMAKE_BINARY_DIR}/test/config.ini"
            USES_TERMINAL
            VERBATIM
            COMMENT "Running Gridcoin functional test suite"
        )
    else()
        message(WARNING "Python3 interpreter not found; the `functional_tests` CTest case will not be registered. Install Python 3 and reconfigure to enable it.")
    endif()
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
