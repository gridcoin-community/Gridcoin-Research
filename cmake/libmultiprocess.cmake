# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

# Add the vendored libmultiprocess subtree (src/ipc/libmultiprocess) as a build
# subdirectory, producing the runtime library (multiprocess) and the code
# generator (mpgen) in-tree. Adapted from Bitcoin Core's cmake/libmultiprocess.cmake
# for Gridcoin, which has no core_interface target and gates tests on ENABLE_TESTS
# (Core uses BUILD_TESTS).
function(add_libmultiprocess subdir)
  # Build libmultiprocess's own suite (mptest) with Gridcoin's tests on native
  # Linux builds, which include every CI sanitizer job. BUILD_TESTING is the
  # standard CMake option the subtree keys its test targets on; mptest also needs
  # Cap'n Proto's kj-test library, and the subtree defines it only when that is
  # found. It is left out of:
  #   * Windows: its spawn and listen tests include <sys/wait.h> and
  #     <sys/socket.h>, and the mingw depends build of Cap'n Proto provides
  #     kj-test, so it would be defined and fail to compile;
  #   * cross builds, where ctest would run it under an emulator;
  #   * macOS, where it has not been run.
  if(ENABLE_TESTS AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CROSSCOMPILING)
    set(BUILD_TESTING ON)
  else()
    set(BUILD_TESTING OFF)
  endif()

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

  # The subtree is EXCLUDE_FROM_ALL, so put mptest back in "all": ctest runs the
  # executable, it does not build it.
  if(TARGET mptest)
    set_target_properties(mptest PROPERTIES EXCLUDE_FROM_ALL OFF)
  endif()

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
