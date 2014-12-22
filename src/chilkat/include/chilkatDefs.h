
#ifndef _CHILKATDEFS_H_INCLUDED_
#define _CHILKATDEFS_H_INCLUDED_

// If this is Windows Phone, then define CK_LIBWINPHONE
#if !defined(CK_LIBWINPHONE)
#if defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#define CK_LIBWINPHONE
#endif
#endif
	
#if !defined(CK_LIBWINSTORE)
#if defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PC_APP
#define CK_LIBWINSTORE
#endif
#endif


// Is this a Windows platform of any type?
#if !defined(CK_WINDOWS)
#if defined(WIN32) || defined(_WIN32) || defined(_WINDOWS) || defined(_Windows) || defined(WINCE) || defined(WINAPI_FAMILY) || defined(__MINGW32__) || defined(__WIN32__) || defined(_WIN64)
#define CK_WINDOWS
#endif
#endif
	

#include "ck_inttypes.h"

#if defined(CK_WINDOWS) 
typedef unsigned long ckUInt32;
#else
typedef uint32_t ckUInt32;
#define CK_USE_UINT_T
#endif
	

// Determine if Mailman SMTPQ related methods are included.
// SMTPQ is included for Windows-based builds that are not CE or Windows Phone
#if defined(CK_WINDOWS) && !defined(WINCE) && !defined(CK_LIBWINPHONE) && !defined(CK_LIBWINSTORE)
#define CK_SMTPQ_INCLUDED
#endif

// Determine if the SFX (Self-extracting EXE) functionality is included.
#if defined(CK_WINDOWS) && !defined(WINCE) && !defined(CK_LIBWINPHONE) && !defined(LINKAGE_TESTING) && !defined(CK_LIBWINSTORE)
#define CK_SFX_INCLUDED
#endif

// Determine if Windows-based Cryptographic Service Provider (CSP) functionality is available
#if defined(CK_WINDOWS) && !defined(CK_LIBWINPHONE) && !defined(__MINGW32__) && !defined(CK_LIBWINSTORE)
#define CK_CSP_INCLUDED
#endif

// Determine if Windows-based CryptoAPI related functionality is available
// These are methods that entirely depend on an underlying MS Crypto API implementation.
#if defined(CK_WINDOWS) && !defined(__MINGW32__) && !defined(CK_LIBWINPHONE) && !defined(CK_LIBWINSTORE)
#define CK_CRYPTOAPI_INCLUDED
#endif

// Determine if methods relating to the Windows registry-based certificate stores are included.
#if defined(CK_WINDOWS) && !defined(__MINGW32__) && !defined(CK_LIBWINPHONE) && !defined(CK_LIBWINSTORE)
#define CK_WINCERTSTORE_INCLUDED
#endif

// Determine if MX DNS lookups are available.
// -We are missing MX DNS lookups on the following systems:
#if !defined(WINCE) && !defined(__MINGW32__) && !defined(CK_LIBWINPHONE) && !defined(CK_LIBWINSTORE)
#define CK_MX_INCLUDED
#endif

#include "SystemTime.h"
#include "FileTime.h"

#if defined(__BORLANDC__) && defined(__WIN32__)
#pragma link "ws2_32.lib"
#pragma link "crypt32.lib"
#endif


#if !defined(CK_VISIBLE_PUBLIC)
#if defined(CK_WINDOWS)
    #define CK_VISIBLE_PUBLIC
    #define CK_VISIBLE_PRIVATE
#else
  #if __GNUC__ >= 4
    #define CK_VISIBLE_PUBLIC __attribute__ ((visibility ("default")))
    #define CK_VISIBLE_PRIVATE  __attribute__ ((visibility ("hidden")))
  #else
    #define CK_VISIBLE_PUBLIC
    #define CK_VISIBLE_PRIVATE
  #endif
#endif
#endif

#endif  // _CHILKATDEFS_H_INCLUDED_

