// This header is NOT generated.

#if !defined(_CKCERTCOLLECTION_H_INCLUDED_)
#define _CKCERTCOLLECTION_H_INCLUDED_


class CkCert;
class CkString;
#include "CkMultiByteBase.h"
#include "CkString.h"

/*
    IMPORTANT: Objects returned by methods as non-const pointers must be deleted
    by the calling application. 

  */

#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// Holds one or more certificates.
// CLASS: CkCertCollection
class CkCertCollection  : public CkMultiByteBase
{
    private:
	// Don't allow assignment or copying these objects.
	CkCertCollection(const CkCertCollection &) { } 
	CkCertCollection &operator=(const CkCertCollection &) { return *this; }
	CkCertCollection(void *impl);

    public:
	CkCertCollection();
	virtual ~CkCertCollection();

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	// BEGIN PUBLIC INTERFACE


	bool LoadPkcs7File(const char *path);
	bool LoadPkcs7Data(const unsigned char *pByteData, unsigned long szByteData);

	CkCert *GetCert(int index);

	int get_NumCerts(void) const;


	// CK_INSERT_POINT

	// END PUBLIC INTERFACE



};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif // !defined(_CKCERTCOLLECTION_H_INCLUDED_)

