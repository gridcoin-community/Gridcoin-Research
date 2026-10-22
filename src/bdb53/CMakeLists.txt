include(ExternalProject)

set(LIBDB_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist")
set(LIBDB_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/libdb_build")
set(libdb_cxx_library "${LIBDB_BUILD_DIR}/libdb_cxx.a")
set_directory_properties(PROPERTIES CLEAN_NO_CUSTOM 1)


# Configure flags
# ===============

set(BDB_FLAGS
    --disable-java
    --disable-jdbc
    --disable-replication
    --enable-cxx
)
if(ENABLE_PIE)
    list(APPEND BDB_FLAGS
        --with-pic
    )
endif()
if(WIN32)
    # configure script doesn't support DOS-style paths
    cmake_path(SET relative_dist_dir "${LIBDB_DIST_DIR}")
    cmake_path(RELATIVE_PATH relative_dist_dir BASE_DIRECTORY "${LIBDB_BUILD_DIR}")
    list(APPEND BDB_FLAGS
        --enable-mingw
        --srcdir "${relative_dist_dir}"
    )
endif()

# Compiler and compile flags
# ==========================
#
# BDB is built by its own configure script, which sees nothing CMake applies to
# this tree's targets unless it is passed here.
#
# CFLAGS and CXXFLAGS on a configure line REPLACE autoconf's default "-g -O2";
# they do not add to it. The warning suppressions below were each added that
# way, and each left BDB compiled with no optimisation at all, on every build
# that uses this copy.
#
# So BDB gets the tree's optimisation, debug-info and debug-path flags (-O*,
# -g*, -f{file,debug,macro}-prefix-map=), taken from CMAKE_<LANG>_FLAGS and
# CMAKE_<LANG>_FLAGS_<CONFIG> together, because builds put them in either:
# Release carries -O2 in the per-config flags, while debhelper's default build
# type is None, with dpkg's -g -O2 in the base flags. The prefix maps go with
# -g, or BDB's debug info would carry absolute build paths that the rest of a
# reproducible build maps away. BDB is optimised in an optimised build, and
# unoptimised only where the tree is (Debug, or no build type and no -O). Only
# those flags: the base flags also carry sanitizer instrumentation (the CI
# sanitizer jobs), distribution hardening with -Werror= among it
# (dpkg-buildflags), and -std= (depends), none of which BDB 5.3 has ever been
# built with. No -DNDEBUG is passed, so BDB's assert state is what it always
# was, as the tree's live-assert policy wants.
#
# A multi-config generator has no build type here, so BDB, configured once for
# every configuration, gets only what the base flags carry. Nothing in this
# tree uses one; say so rather than build BDB unoptimised in silence.
#
# CC and CXX are the compilers this tree was configured with, including any
# arguments CMake split off them into CMAKE_<LANG>_COMPILER_ARG1: depends gives
# "gcc -m64", and its darwin compiler carries --target and -isysroot. Without
# CC and CXX, configure takes whatever cc and g++ come first on PATH.
#
# Except for a compiler, the SDK below, or a prefix map whose path contains
# whitespace. configure runs CC, CXX and the flags as word lists ($CC -c
# $CFLAGS ...), so it would split such a path and fail ("C compiler cannot
# create executables"). Such a compiler leaves CC and CXX out, such an SDK
# leaves the compiler's default, and such a prefix map is left out, so that
# path stays unmapped in BDB's debug info; each with a warning.
#
# On Apple, CMake adds the deployment target, SDK and architecture to its own
# compiles from CMAKE_OSX_*, not to any flags variable, so without them BDB is
# built for the build machine's macOS. The Intel release job, building for 14.0
# on a macOS 15 runner, linked BDB objects built for 15.0.
function(bdb_tree_flags out lang)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" config)
    separate_arguments(tree_flags UNIX_COMMAND "${CMAKE_${lang}_FLAGS} ${CMAKE_${lang}_FLAGS_${config}}")
    set(kept "")
    foreach(flag IN LISTS tree_flags)
        if(NOT flag MATCHES "^-(O|g|f(file|debug|macro)-prefix-map=)")
            continue()
        elseif(flag MATCHES "[ \t]")
            message(WARNING "Berkeley DB (bundled): its configure cannot take a flag containing whitespace "
                            "(${flag}), so BDB's ${lang} compiles leave it out.")
        else()
            list(APPEND kept "${flag}")
        endif()
    endforeach()
    list(JOIN kept " " kept)
    set(${out} "${kept}" PARENT_SCOPE)
endfunction()

get_property(bdb_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(bdb_multi_config)
    message(WARNING "Berkeley DB (bundled): the ${CMAKE_GENERATOR} generator has no single build type, "
                    "so BDB gets only the optimisation the base flags carry, if any. Use a single-config "
                    "generator (Unix Makefiles, Ninja) to build it optimised.")
endif()

bdb_tree_flags(BDB_C_FLAGS C)
bdb_tree_flags(BDB_CXX_FLAGS CXX)

# C only: both families reject parts of BDB 5.3 under their current defaults.
# GCC 14 made implicit function declarations an error, and several of BDB's
# configure probes rely on them. Failing, those probes leave an x86-64 GCC
# build without the assembly mutexes and atomics that a Clang or GCC 13 build
# gets, falling back to pthread mutexes and no atomics.
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    string(APPEND BDB_C_FLAGS " -Wno-implicit-function-declaration -Wno-error=implicit-int -Wno-incompatible-pointer-types")
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    string(APPEND BDB_C_FLAGS " -Wno-error=implicit-function-declaration -Wno-error=implicit-int -Wno-incompatible-pointer-types")
endif()

if(APPLE)
    set(bdb_apple_flags "")
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        string(APPEND bdb_apple_flags " -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
    # Only a path: CMAKE_OSX_SYSROOT may also hold an SDK name, which -isysroot
    # does not take. Empty leaves the compiler's default SDK, as for the tree.
    if(CMAKE_OSX_SYSROOT MATCHES "[ \t]")
        message(WARNING "Berkeley DB (bundled): its configure cannot take an SDK path containing whitespace "
                        "(${CMAKE_OSX_SYSROOT}), so BDB is built against the compiler's default SDK.")
    elseif(IS_DIRECTORY "${CMAKE_OSX_SYSROOT}")
        string(APPEND bdb_apple_flags " -isysroot ${CMAKE_OSX_SYSROOT}")
    endif()
    # One architecture at most: configure runs the preprocessor ($CC -E), which
    # clang refuses with more than one -arch. Before -arch was passed at all, a
    # universal build compiled BDB for the build machine alone and failed at link.
    list(LENGTH CMAKE_OSX_ARCHITECTURES bdb_arch_count)
    if(bdb_arch_count GREATER 1)
        message(FATAL_ERROR "The bundled Berkeley DB cannot be built for several architectures at once "
                            "(CMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}). Build each architecture "
                            "separately.")
    elseif(bdb_arch_count EQUAL 1)
        string(APPEND bdb_apple_flags " -arch ${CMAKE_OSX_ARCHITECTURES}")
    endif()
    string(APPEND BDB_C_FLAGS "${bdb_apple_flags}")
    string(APPEND BDB_CXX_FLAGS "${bdb_apple_flags}")
endif()

string(STRIP "${BDB_C_FLAGS}" BDB_C_FLAGS)
string(STRIP "${BDB_CXX_FLAGS}" BDB_CXX_FLAGS)
if(CMAKE_C_COMPILER MATCHES "[ \t]" OR CMAKE_CXX_COMPILER MATCHES "[ \t]")
    message(WARNING "Berkeley DB (bundled): its configure cannot run a compiler whose path contains whitespace "
                    "(${CMAKE_C_COMPILER}, ${CMAKE_CXX_COMPILER}), so BDB is built with the compilers on PATH.")
    set(BDB_CC "(from PATH)")
    set(BDB_CXX "(from PATH)")
else()
    string(STRIP "${CMAKE_C_COMPILER} ${CMAKE_C_COMPILER_ARG1}" BDB_CC)
    string(STRIP "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1}" BDB_CXX)
    list(APPEND BDB_FLAGS
        "CC=${BDB_CC}"
        "CXX=${BDB_CXX}"
    )
endif()

list(APPEND BDB_FLAGS
    "CFLAGS=${BDB_C_FLAGS}"
    "CXXFLAGS=${BDB_CXX_FLAGS}"
)
if(CMAKE_CROSSCOMPILING)
    list(APPEND BDB_FLAGS
        "--host=${CMAKE_C_COMPILER_TARGET}"
        "AR=${CMAKE_AR}"
        "RANLIB=${CMAKE_RANLIB}"
    )
endif()

# The sub-build's own output goes to log files in the build tree (LOG_BUILD
# below), so state here what it is given; CI logs then show it.
message(STATUS "Berkeley DB (bundled): CC=${BDB_CC}")
message(STATUS "Berkeley DB (bundled): CXX=${BDB_CXX}")
message(STATUS "Berkeley DB (bundled): CFLAGS=${BDB_C_FLAGS}")
message(STATUS "Berkeley DB (bundled): CXXFLAGS=${BDB_CXX_FLAGS}")

# Make flags
# ==========

include(ProcessorCount)
ProcessorCount(N)
if(N EQUAL 0)
    set(N 1)
endif()

set(MAKEOPTS "-j${N}" CACHE STRING "Options for the 'make' program")


# External project
# ================

ExternalProject_Add(BerkeleyDB_Project
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    BINARY_DIR ${LIBDB_BUILD_DIR}
    CONFIGURE_COMMAND ${SH_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/dist/configure ${BDB_FLAGS}
    BUILD_COMMAND
        COMMAND ${MAKE_EXE} ${MAKEOPTS} clean
        COMMAND ${MAKE_EXE} ${MAKEOPTS} libdb_cxx.a
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${libdb_cxx_library}
    LOG_CONFIGURE TRUE
    LOG_BUILD TRUE
    LOG_OUTPUT_ON_FAILURE TRUE
)

add_library(libdb_cxx STATIC IMPORTED GLOBAL)
add_dependencies(libdb_cxx BerkeleyDB_Project)

set_target_properties(libdb_cxx PROPERTIES
    IMPORTED_LOCATION ${libdb_cxx_library}
)
target_include_directories(libdb_cxx INTERFACE ${LIBDB_BUILD_DIR})
