// CkCreateCSW.h: interface for the CkCreateCSW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkCreateCSW_H
#define _CkCreateCSW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkCertStoreW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkCreateCSW
class CK_VISIBLE_PUBLIC CkCreateCSW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkCreateCSW(const CkCreateCSW &);
	CkCreateCSW &operator=(const CkCreateCSW &);

    public:
	CkCreateCSW(void);
	virtual ~CkCreateCSW(void);

	static CkCreateCSW *createNew(void);
	

	
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
	CkCertStoreW *CreateFileStore(const wchar_t *path);

	// Creates a temporary certificate store in memory that is initially empty.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStoreW *CreateMemoryStore(void);

	// Creates a registry-based certificate store. The 'hiveName' can either be
	// "CurrentUser" or "LocalMachine". The 'regPath' argument should be specified
	// without a leading slash, such as "Software/Chilkat/MyCertStore".
	// The caller is responsible for deleting the object returned by this method.
	CkCertStoreW *CreateRegistryStore(const wchar_t *regRoot, const wchar_t *regPath);

	// Opens the certificate store used by Chilkat Mail and returns the object
	// representing that store.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStoreW *OpenChilkatStore(void);

	// Opens the local system's Current User Certificate Store and returns the object
	// representing that store.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStoreW *OpenCurrentUserStore(void);

	// Opens an existing file certificate store. To open it read-only, readOnly should
	// be non-zero.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStoreW *OpenFileStore(const wchar_t *path);

	// Opens the local system's Local Machine Certificate Store and returns the object
	// representing that store.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStoreW *OpenLocalSystemStore(void);

	// Opens the certificate store used by Microsoft Outlook (and Office) and returns
	// the object representing that store.
	// The caller is responsible for deleting the object returned by this method.
	CkCertStoreW *OpenOutlookStore(void);

	// Opens an existing registry-based certificate store. 'hiveName' should be either
	// "CurrentUser" or "LocalMachine". The 'regPath' argument should be specified
	// without a leading slash, such as "Software/Chilkat/MyCertStore".
	// The caller is responsible for deleting the object returned by this method.
	CkCertStoreW *OpenRegistryStore(const wchar_t *regRoot, const wchar_t *regPath);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
