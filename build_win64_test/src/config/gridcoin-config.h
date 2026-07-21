#ifndef GRIDCOIN_CONFIG_H
#define GRIDCOIN_CONFIG_H

#define PACKAGE_NAME "Gridcoin"
#define PACKAGE_VERSION "5.5.1.4"
#define PACKAGE_BUGREPORT "https://github.com/gridcoin-community/Gridcoin-Research/issues"
#define CLIENT_VERSION_MAJOR 5
#define CLIENT_VERSION_MINOR 5
#define CLIENT_VERSION_REVISION 1
#define CLIENT_VERSION_BUILD 4
#define CLIENT_VERSION_IS_RELEASE false
#define COPYRIGHT_YEAR "2026"
#define COPYRIGHT_HOLDERS_FINAL "The Gridcoin developers"

#ifndef ENABLE_SSE41
#define ENABLE_SSE41
#endif

#ifndef ENABLE_AVX2
#define ENABLE_AVX2
#endif

#ifndef ENABLE_X86_SHANI
#define ENABLE_X86_SHANI
#endif

#ifndef ENABLE_ARM_CRC
/* #undef ENABLE_ARM_CRC */
#endif

#ifndef ENABLE_ARM_SHANI
/* #undef ENABLE_ARM_SHANI */
#endif

#define USE_ASM
#define USE_ASM_SCRYPT

/* #undef USE_DBUS */

/* #undef HAVE_STRERROR_R */
/* #undef STRERROR_R_CHAR_P */

/* #undef WORDS_BIGENDIAN */

/* #undef HAVE_BYTESWAP_H */
/* #undef HAVE_ENDIAN_H */
/* #undef HAVE_SYS_ENDIAN_H */
/* #undef HAVE_SYS_PRCTL_H */

#define HAVE_DECL_FORK 0
#define HAVE_DECL_PIPE2 0
#define HAVE_DECL_SETSID 0

#define HAVE_DECL_LE16TOH 0
#define HAVE_DECL_LE32TOH 0
#define HAVE_DECL_LE64TOH 0

#define HAVE_DECL_HTOLE16 0
#define HAVE_DECL_HTOLE32 0
#define HAVE_DECL_HTOLE64 0

#define HAVE_DECL_BE16TOH 0
#define HAVE_DECL_BE32TOH 0
#define HAVE_DECL_BE64TOH 0

#define HAVE_DECL_HTOBE16 0
#define HAVE_DECL_HTOBE32 0
#define HAVE_DECL_HTOBE64 0

#define HAVE_DECL_BSWAP_16 0
#define HAVE_DECL_BSWAP_32 0
#define HAVE_DECL_BSWAP_64 0

/* #undef HAVE_BUILTIN_CLZL */
/* #undef HAVE_BUILTIN_CLZLL */

/* #undef HAVE_MSG_NOSIGNAL */
/* #undef HAVE_MSG_DONTWAIT */

/* #undef HAVE_MALLOC_INFO */
/* #undef HAVE_MALLOPT_ARENA_MAX */

#define HAVE_SYSTEM 1
/* #undef HAVE_GMTIME_R */

/* #undef HAVE_GETRANDOM */
/* #undef HAVE_GETENTROPY_RAND */
/* #undef HAVE_SYSCTL */
/* #undef HAVE_SYSCTL_ARND */

#define HAVE_O_CLOEXEC 0
/* #undef HAVE_STRONG_GETAUXVAL */

#endif //GRIDCOIN_CONFIG_H
