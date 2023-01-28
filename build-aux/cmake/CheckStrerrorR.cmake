include(CheckCSourceRuns)
include(CheckSymbolExists)

check_symbol_exists(strerror_r "string.h" HAVE_STRERROR_R)

if(HAVE_STRERROR_R)
    check_c_source_runs("
        #include <errno.h>
        #include <string.h>
        #include <stdio.h>

        int main() {
            char buf[280];
            char* s = strerror_r(ENOENT, buf, 280);
            printf(\"%s\", s);
            return 0;
        }"
        STRERROR_R_CHAR_P
    )
endif()
