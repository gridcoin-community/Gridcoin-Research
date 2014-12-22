// CkRar.h: interface for the CkRar class.
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

#ifndef _CKRAR_H
#define _CKRAR_H


#include "CkString.h"
class CkByteData;
//class CkRarProgress;
class CkRarEntry;
class CkStringArray;
#include "CkMultiByteBase.h"

/*
    IMPORTANT: Objects returned by methods as non-const pointers must be deleted
    by the calling application. 

*/
#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkRar
class CkRar  : public CkMultiByteBase
{
    public:
	CkRar();
	virtual ~CkRar();

	// BEGIN PUBLIC INTERFACE

	bool Unrar(const char *dirPath);

	bool FastOpen(const char *path);

	bool Open(const char *path);

	bool Close(void);

	long get_NumEntries(void);
	CkRarEntry *GetEntryByName(const char *filename);
	CkRarEntry *GetEntryByIndex(long index);



	// CK_INSERT_POINT

	// END PUBLIC INTERFACE



    // Everything below is for internal use only.
    private:
	// Don't allow assignment or copying these objects.
	CkRar(const CkRar &);
	CkRar &operator=(const CkRar &);
	CkRar(void *impl);


};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif

#endif
