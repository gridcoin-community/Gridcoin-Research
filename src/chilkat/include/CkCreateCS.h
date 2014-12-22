// CkCreateCS.h: interface for the CkCreateCS class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkCreateCS_H
#define _CkCreateCS_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkCertStore;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkCreateCS
class CK_VISIBLE_PUBLIC CkCreateCS  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkCreateCS(const CkCreateCS &);
	CkCreateCS &operator=(const CkCreateCS &);

    public:
	CkCreateCS(void);
	virtual ~CkCreateCS(void);

	static CkCreateCS *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// Determines whether certificate stores are opened with read-only or read/write
	// permissions. Only applies to methods such as OpenCurrentUserStore, where the a
	// readOnly parameter is not present.
	bool get_ReadOnly(void);
	// Determines whether certificate stores are opened with read-only or read/write
	// permissions. Only applies to methods such as OpenCurrentUserStore, where the a
	// readOnly parameter is not present.
	void put_ReadOnly(bool newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Creates a file-based certificate store. If 'filename' already exists, the method
	// will fail.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStore *CreateFileStore(const char *path);

	// Creates a temporary certificate store in memory that is initially empty.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStore *CreateMemoryStore(void);

	// Creates a registry-based certificate store. The 'hiveName' can either be
	// "CurrentUser" or "LocalMachine". The 'regPath' argument should be specified
	// without a leading slash, such as "Software/Chilkat/MyCertStore".
	// The caller is responsible for deleting the object returned by this method.
	CkCertStore *CreateRegistryStore(const char *regRoot, const char *regPath);

	// Opens the certificate store used by Chilkat Mail and returns the object
	// representing that store.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStore *OpenChilkatStore(void);

	// Opens the local system's Current User Certificate Store and returns the object
	// representing that store.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStore *OpenCurrentUserStore(void);

	// Opens an existing file certificate store. To open it read-only, readOnly should
	// be non-zero.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStore *OpenFileStore(const char *path);

	// Opens the local system's Local Machine Certificate Store and returns the object
	// representing that store.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStore *OpenLocalSystemStore(void);

	// Opens the certificate store used by Microsoft Outlook (and Office) and returns
	// the object representing that store.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStore *OpenOutlookStore(void);

	// Opens an existing registry-based certificate store. 'hiveName' should be either
	// "CurrentUser" or "LocalMachine". The 'regPath' argument should be specified
	// without a leading slash, such as "Software/Chilkat/MyCertStore".
	// The caller is responsible for deleting the object returned by this method.
	CkCertStore *OpenRegistryStore(const char *regRoot, const char *regPath);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
