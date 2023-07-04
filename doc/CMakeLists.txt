find_package(Doxygen REQUIRED dot)

set(DOXYGEN_PROJECT_BRIEF "P2P Digital Currency")
set(DOXYGEN_PROJECT_LOGO "${CMAKE_SOURCE_DIR}/share/icons/hicolor/48x48/apps/gridcoinresearch.png")
set(DOXYGEN_JAVADOC_AUTOBRIEF "YES")
set(DOXYGEN_TAB_SIZE 8)
set(DOXYGEN_EXTRACT_ALL "YES")
set(DOXYGEN_EXTRACT_PRIVATE "YES")
set(DOXYGEN_EXTRACT_STATIC "YES")
set(DOXYGEN_RECURSIVE "YES")
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/README_doxygen.md")
set(DOXYGEN_SOURCE_BROWSER "YES")

set(DOXYGEN_EXCLUDE
    "${CMAKE_SOURCE_DIR}/src/bdb53"
    "${CMAKE_SOURCE_DIR}/src/crc32c"
    "${CMAKE_SOURCE_DIR}/src/leveldb"
    "${CMAKE_SOURCE_DIR}/src/test"
    "${CMAKE_SOURCE_DIR}/src/qt/test"
)

doxygen_add_docs(docs
    "${CMAKE_SOURCE_DIR}/src"
    ALL
)
