find_package(Git)

set(BUILD_GIT_TAG "" CACHE STRING "Current Git tag")
set(BUILD_GIT_COMMIT "" CACHE STRING "Current Git commit")

if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" diff-index --quiet HEAD --
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE BUILD_GIT_IS_DIRTY
    )

    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE BUILD_GIT_COMMIT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(COMMAND "${GIT_EXECUTABLE}" describe --abbrev=0
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE BUILD_GIT_LATEST_TAG
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-list "${BUILD_GIT_LATEST_TAG}" -1 --abbrev-commit --abbrev=12
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE BUILD_GIT_LATEST_TAG_COMMIT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(BUILD_GIT_IS_DIRTY EQUAL 0) # not dirty
        if(BUILD_GIT_LATEST_TAG_COMMIT EQUAL BUILD_GIT_COMMIT)
            set(BUILD_GIT_TAG "${BUILD_GIT_LATEST_TAG}")
        endif()
    else()
        set(BUILD_GIT_COMMIT "${BUILD_GIT_COMMIT}-dirty")
    endif()
endif()

if(BUILD_GIT_TAG EQUAL "")
    unset(BUILD_GIT_TAG)
endif()

if(BUILD_GIT_COMMIT EQUAL "")
    unset(BUILD_GIT_COMMIT)
endif()
