# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/src/bdb53")
  file(MAKE_DIRECTORY "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/src/bdb53")
endif()
file(MAKE_DIRECTORY
  "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/bdb53/libdb_build"
  "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/bdb53/BerkeleyDB_Project-prefix"
  "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/bdb53/BerkeleyDB_Project-prefix/tmp"
  "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/bdb53/BerkeleyDB_Project-prefix/src/BerkeleyDB_Project-stamp"
  "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/bdb53/BerkeleyDB_Project-prefix/src"
  "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/bdb53/BerkeleyDB_Project-prefix/src/BerkeleyDB_Project-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/bdb53/BerkeleyDB_Project-prefix/src/BerkeleyDB_Project-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/bdb53/BerkeleyDB_Project-prefix/src/BerkeleyDB_Project-stamp${cfgdir}") # cfgdir has leading slash
endif()
