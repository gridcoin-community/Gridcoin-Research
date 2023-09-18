# RT_FOUND        - system has shm_open
# RT_LIBRARIES    - libraries needed to use shm_open

include(CheckCSourceCompiles)

set(rt_code "
    #include <sys/types.h>
    #include <sys/mman.h>

    int main() {
        shm_open(0, 0, 0);
        return 0;
    }"
)

check_c_source_compiles("${rt_code}" RT_BUILT_IN)

if(RT_BUILT_IN)
    set(RT_FOUND TRUE)
    set(RT_LIBRARIES)
else()
    set(CMAKE_REQUIRED_LIBRARIES "-lrt")
    check_c_source_compiles("${rt_code}" RT_IN_LIBRARY)
    set(CMAKE_REQUIRED_LIBRARIES)
    if(RT_IN_LIBRARY)
        set(RT_LIBRARY rt)
        include(FindPackageHandleStandardArgs)
        find_package_handle_standard_args(Rt DEFAULT_MSG RT_LIBRARY)
        set(RT_LIBRARIES ${RT_LIBRARY})
        unset(RT_LIBRARY)
    endif()
endif()
