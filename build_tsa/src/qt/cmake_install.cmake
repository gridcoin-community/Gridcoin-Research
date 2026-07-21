# Install script for directory: /home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/src/qt

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
  include("/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/qt/locale/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "gui" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearch" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearch")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearch"
         RPATH "")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/bin/gridcoinresearch")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearch" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearch")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/llvm-strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/gridcoinresearch")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "gui" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share" TYPE DIRECTORY FILES "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/share/icons")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "gui" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications" TYPE FILE FILES "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/contrib/gridcoinresearch.desktop")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "gui" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man1" TYPE FILE FILES "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/doc/gridcoinresearch.1")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "gui" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/bash-completion/completions" TYPE FILE RENAME "gridcoinresearch" FILES "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/contrib/completions/bash/gridcoinresearch.bash")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "gui" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/metainfo" TYPE FILE FILES "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/qt/us.gridcoin.GridcoinResearch.metainfo.xml")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa/src/qt/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
