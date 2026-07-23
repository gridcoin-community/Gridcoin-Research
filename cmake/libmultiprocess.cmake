# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

# Add the vendored libmultiprocess subtree (src/ipc/libmultiprocess) as a build
# subdirectory, producing the runtime library (multiprocess) and the code
# generator (mpgen) in-tree. Adapted from Bitcoin Core's cmake/libmultiprocess.cmake
# for Gridcoin, which has no core_interface target and gates tests on ENABLE_TESTS
# (Core uses BUILD_TESTS).
function(add_libmultiprocess subdir)
  # Do not build libmultiprocess's own test and example targets inside the
  # Gridcoin build; keeping BUILD_TESTING OFF means those targets (mptest,
  # mpexample, ...) are never defined, so we must not reference them below.
  set(BUILD_TESTING OFF)

  # libmultiprocess requires C++20 (concepts). It only sets CMAKE_CXX_STANDARD
  # itself when it is the top-level project; embedded via add_subdirectory it
  # inherits the parent standard, and Gridcoin is C++17. Bump the standard to 20
  # for the subtree's translation units (this set() is function-scoped and is
  # inherited by the add_subdirectory child scope below, so it does not change the
  # C++17 standard used by the rest of Gridcoin). The subtree also keys some of its
  # own configuration on CMAKE_CXX_STANDARD EQUAL 20.
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  add_subdirectory(${subdir} EXCLUDE_FROM_ALL)

  # Hide Cap'n Proto's internal imported-location cache variables from the cmake
  # UI; they are set by the subtree's find_package(CapnProto) and are noise.
  mark_as_advanced(CapnProto_DIR)
  mark_as_advanced(CapnProto_capnpc_IMPORTED_LOCATION)
  mark_as_advanced(CapnProto_capnp_IMPORTED_LOCATION)
  mark_as_advanced(CapnProto_capnp-json_IMPORTED_LOCATION)
  mark_as_advanced(CapnProto_capnp-rpc_IMPORTED_LOCATION)
  mark_as_advanced(CapnProto_capnp-websocket_IMPORTED_LOCATION)
  mark_as_advanced(CapnProto_kj-async_IMPORTED_LOCATION)
  mark_as_advanced(CapnProto_kj-gzip_IMPORTED_LOCATION)
  mark_as_advanced(CapnProto_kj-http_IMPORTED_LOCATION)
  mark_as_advanced(CapnProto_kj_IMPORTED_LOCATION)
  mark_as_advanced(CapnProto_kj-test_IMPORTED_LOCATION)
  mark_as_advanced(CapnProto_kj-tls_IMPORTED_LOCATION)
endfunction()
