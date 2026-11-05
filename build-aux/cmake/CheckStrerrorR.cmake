include(CheckCXXSourceCompiles)
include(CheckSymbolExists)

check_symbol_exists(strerror_r "string.h" HAVE_STRERROR_R)

if(HAVE_STRERROR_R)
    # SysErrorString() is C++, so the probe must be C++ too. The compiler for C++ defines _GNU_SOURCE (g++ does so
    # unconditionally), which selects the GNU strerror_r that returns a char*. A C probe without _GNU_SOURCE sees
    # the XSI strerror_r that returns an int instead, so it does not describe what the C++ code is compiled against.
    # Compiling is enough to tell the two apart: only the GNU variant converts to char*. Nothing is run, so this
    # also works when cross-compiling.
    check_cxx_source_compiles("
        #include <string.h>

        int main() {
            char buf[280];
            char* s = strerror_r(0, buf, sizeof(buf));
            return s == nullptr;
        }"
        STRERROR_R_CHAR_P_CXX
    )

    if(STRERROR_R_CHAR_P_CXX)
        set(STRERROR_R_CHAR_P 1)
    else()
        set(STRERROR_R_CHAR_P "")
    endif()
endif()
