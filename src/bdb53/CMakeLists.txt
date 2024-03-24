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
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND BDB_FLAGS
        CFLAGS=-Wno-implicit-function-declaration
    )
endif()


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
