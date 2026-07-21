set(CMAKE_SOURCE_DIR "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1")
set(CMAKE_BINARY_DIR "/home/jco/builds/Gridcoin-Research/.claude/worktrees/gui-interfaces-1c1/build_tsa")

if(CPACK_GENERATOR MATCHES "NSIS")
    set(CPACK_PACKAGE_FILE_NAME "gridcoin-${CPACK_PACKAGE_VERSION}-win${CPACK_WINDOWS_BITS}-setup")
endif()

if(CPACK_GENERATOR MATCHES "DragNDrop")
    set(CPACK_PACKAGE_FILE_NAME "gridcoin-${CPACK_PACKAGE_VERSION}-macos-x86_64")
endif()
