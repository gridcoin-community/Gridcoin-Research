set(LIBGRIDCOIN_CRYPTO gridcoin_crypto_base)
add_library(gridcoin_crypto_base STATIC
    aes.cpp
    chacha20.cpp
    hmac_sha256.cpp
    hmac_sha512.cpp
    poly1305.cpp
    ripemd160.cpp
    sha1.cpp
    sha256.cpp
    sha3.cpp
    sha512.cpp
    siphash.cpp
    ../gridcoin/md5.c
)

if(USE_ASM)
    target_sources(gridcoin_crypto_base PRIVATE sha256_sse4.cpp)
endif()

if(ENABLE_SSE41)
    list(APPEND LIBGRIDCOIN_CRYPTO gridcoin_crypto_sse41)
    add_library(gridcoin_crypto_sse41 STATIC sha256_sse41.cpp)
    target_compile_definitions(gridcoin_crypto_sse41 PRIVATE ENABLE_SSE41)
    if(NOT MSVC)
        target_compile_options(gridcoin_crypto_sse41 PRIVATE -msse4.1)
    endif()
endif()

if(ENABLE_AVX2)
    list(APPEND LIBGRIDCOIN_CRYPTO gridcoin_crypto_avx2)
    add_library(gridcoin_crypto_avx2 STATIC sha256_avx2.cpp)
    target_compile_definitions(gridcoin_crypto_avx2 PRIVATE ENABLE_AVX2)
    if(NOT MSVC)
        target_compile_options(gridcoin_crypto_avx2 PRIVATE -mavx -mavx2)
    endif()
endif()

if(ENABLE_X86_SHANI)
    list(APPEND LIBGRIDCOIN_CRYPTO gridcoin_crypto_x86_shani)
    add_library(gridcoin_crypto_x86_shani STATIC sha256_x86_shani.cpp)
    target_compile_definitions(gridcoin_crypto_x86_shani PRIVATE ENABLE_X86_SHANI)
    if(NOT MSVC)
        target_compile_options(gridcoin_crypto_x86_shani PRIVATE -msse4 -msha)
    endif()
endif()

if(ENABLE_ARM_SHANI)
    list(APPEND LIBGRIDCOIN_CRYPTO gridcoin_crypto_arm_shani)
    add_library(gridcoin_crypto_arm_shani STATIC sha256_arm_shani.cpp)
    target_compile_definitions(gridcoin_crypto_arm_shani PRIVATE ENABLE_ARM_SHANI)
endif()

foreach(library IN LISTS LIBGRIDCOIN_CRYPTO)
    target_include_directories(${library} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_BINARY_DIR}/src"
    )
    target_compile_definitions(${library} PRIVATE HAVE_CONFIG_H)
endforeach()

set(LIBGRIDCOIN_CRYPTO ${LIBGRIDCOIN_CRYPTO}
    CACHE INTERNAL "Gridcoin crypto libraries"
)
