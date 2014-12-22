// CkPfxW.h: interface for the CkPfxW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkPfxW_H
#define _CkPfxW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkCertW;
class CkPrivateKeyW;
class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkPfxW
class CK_VISIBLE_PUBLIC CkPfxW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkPfxW(const CkPfxW &);
	CkPfxW &operator=(const CkPfxW &);

    public:
	CkPfxW(void);
	virtual ~CkPfxW(void);

	static CkPfxW *createNew(void);
	

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The number of certificates contained in the PFX.
	int get_NumCerts(void);

	// The number of private keys contained in the PFX.
	int get_NumPrivateKeys(void);



	// ----------------------
	// Methods
	// ----------------------
	// Returns the Nth certificate in the PFX. (The 1st certificate is at index 0.)
	// The caller is responsible for deleting the object returned by this method.
	CkCertW *GetCert(int index);

	// Returns the Nth private key in the PFX. (The 1st private key is at index 0.)
	// The caller is responsible for deleting the object returned by this method.
	CkPrivateKeyW *GetPrivateKey(int index);

	// Loads a PFX from in-memory bytes.
	bool LoadPfxBytes(const CkByteData &pfxData, const wchar_t *password);

	// Loads a PFX from encoded byte data. The ARG2 can by any encoding, such as
	// "Base64", "modBase64", "Base32", "UU", "QP" (for quoted-printable), "URL" (for
	// url-encoding), "Hex", "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", and
	// "url_rfc3986".
	bool LoadPfxEncoded(const wchar_t *encodedData, const wchar_t *encoding, const wchar_t *password);

	// Loads a PFX from a file.
	bool LoadPfxFile(const wchar_t *path, const wchar_t *password);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
