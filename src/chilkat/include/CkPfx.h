// CkPfx.h: interface for the CkPfx class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkPfx_H
#define _CkPfx_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkCert;
class CkPrivateKey;
class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkPfx
class CK_VISIBLE_PUBLIC CkPfx  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkPfx(const CkPfx &);
	CkPfx &operator=(const CkPfx &);

    public:
	CkPfx(void);
	virtual ~CkPfx(void);

	static CkPfx *createNew(void);
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
	CkCert *GetCert(int index);

	// Returns the Nth private key in the PFX. (The 1st private key is at index 0.)
	// The caller is responsible for deleting the object returned by this method.
	CkPrivateKey *GetPrivateKey(int index);

	// Loads a PFX from in-memory bytes.
	bool LoadPfxBytes(const CkByteData &pfxData, const char *password);

	// Loads a PFX from encoded byte data. The ARG2 can by any encoding, such as
	// "Base64", "modBase64", "Base32", "UU", "QP" (for quoted-printable), "URL" (for
	// url-encoding), "Hex", "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", and
	// "url_rfc3986".
	bool LoadPfxEncoded(const char *encodedData, const char *encoding, const char *password);

	// Loads a PFX from a file.
	bool LoadPfxFile(const char *path, const char *password);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
