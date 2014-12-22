// This header is NOT generated.

#ifndef _CKZIPCRC_H
#define _CKZIPCRC_H

#include "chilkatDefs.h"
	
class CkByteData;
#include "CkString.h"
#include "CkMultiByteBase.h"

#ifdef WIN32
#pragma warning( disable : 4068 )
#pragma unmanaged
#endif


/*
    IMPORTANT: Objects returned by methods as non-const pointers must be deleted
    by the calling application. 

  */

#ifndef __sun__
#pragma pack (push, 8)
#endif
 

#ifndef WIN32
#include <stdint.h>
#endif

// CLASS: CkZipCrc
class CkZipCrc  : public CkMultiByteBase
{
    public:
	CkZipCrc();
	virtual ~CkZipCrc();

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	// BEGIN PUBLIC INTERFACE
	bool ToHex(unsigned long crc, CkString &outStr) const;

	ckUInt32 FileCrc(const char *path);
	ckUInt32 CalculateCrc(const CkByteData &byteData);
	ckUInt32 EndStream(void);

	void MoreData(const CkByteData &byteData);
	void BeginStream(void);

	const char *toHex(unsigned long crc);

	// CK_INSERT_POINT

	// END PUBLIC INTERFACE


    // For internal use only.
    private:
	// Don't allow assignment or copying these objects.
	CkZipCrc(const CkZipCrc &) { } 
	CkZipCrc &operator=(const CkZipCrc &) { return *this; }

    public:
	CkZipCrc(void *impl);


};
#ifndef __sun__
#pragma pack (pop)
#endif



#endif  // _CKZIPCRC_H


