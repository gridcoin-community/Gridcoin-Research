// CkRarEntry.h: interface for the CkRarEntry class.
//
//////////////////////////////////////////////////////////////////////

// This source file is NOT generated.

// If this is Windows Phone, then define CK_LIBWINPHONE
#if !defined(CK_LIBWINPHONE)
#if defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#define CK_LIBWINPHONE
#endif
#endif

#if defined(WIN32) && !defined(__MINGW32__) && !defined(WINCE) && !defined(CK_LIBWINPHONE)

#ifndef _CKRARENTRY__H
#define _CKRARENTRY__H



#include "CkString.h"
class CkByteData;
class CkRar;
#include "CkMultiByteBase.h"

#if !defined(WIN32) && !defined(WINCE)
#include "SystemTime.h"
#endif

#undef Copy

/*
    IMPORTANT: Objects returned by methods as non-const pointers must be deleted
    by the calling application. 

*/
#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkRarEntry
class CkRarEntry  : public CkMultiByteBase
{
    public:

	// BEGIN PUBLIC INTERFACE

	// This entry's filename.
	void get_Filename(CkString &str);

	// The uncompressed size in bytes.
	unsigned long get_UncompressedSize(void);

	// The compressed size in bytes.
	unsigned long get_CompressedSize(void);

	// The entry's file date/time.
	void get_LastModified(SYSTEMTIME &outSysTime);

	// True if this entry is a directory.
	bool get_IsDirectory(void);
	
	bool get_IsReadOnly(void);

	// Extracts this entry into the specified directory. 
	bool Unrar(const char *dirPath);

	unsigned long get_Crc(void);

	const char *filename(void);

	// CK_INSERT_POINT

	// END PUBLIC INTERFACE


    // Everything below here is for internal use only.
    private:

	// Don't allow assignment or copying these objects.
	CkRarEntry(const CkRarEntry &);
	CkRarEntry &operator=(const CkRarEntry &);


    public:

	CkRarEntry();
	CkRarEntry(void *impl);

	virtual ~CkRarEntry();


};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif

#endif

