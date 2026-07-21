# Install script for directory: /home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/llvm-objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/bdb53/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/leveldb/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/univalue/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/crypto/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "daemon" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearchd" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearchd")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearchd"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/bin/gridcoinresearchd")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearchd" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearchd")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/llvm-strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearchd")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "daemon" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/CMakeFiles/gridcoinresearchd.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "daemon" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man1" TYPE FILE FILES "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/doc/gridcoinresearchd.1")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "daemon" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/systemd/system" TYPE FILE FILES "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/contrib/init/gridcoinresearchd.service")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "daemon" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/bash-completion/completions" TYPE FILE RENAME "gridcoinresearchd" FILES "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/contrib/completions/bash/gridcoinresearchd.bash")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/qt/cmake_install.cmake")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
