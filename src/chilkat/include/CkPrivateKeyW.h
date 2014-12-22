// CkPrivateKeyW.h: interface for the CkPrivateKeyW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkPrivateKeyW_H
#define _CkPrivateKeyW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkPrivateKeyW
class CK_VISIBLE_PUBLIC CkPrivateKeyW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkPrivateKeyW(const CkPrivateKeyW &);
	CkPrivateKeyW &operator=(const CkPrivateKeyW &);

    public:
	CkPrivateKeyW(void);
	virtual ~CkPrivateKeyW(void);

	static CkPrivateKeyW *createNew(void);
	

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------


	// ----------------------
	// Methods
	// ----------------------
	// Gets the private key in PKCS8 format.
	bool GetPkcs8(CkByteData &outData);

	// Writes the private key to password-protected PKCS8 format.
	bool GetPkcs8Encrypted(const wchar_t *password, CkByteData &outBytes);

	// Writes the private key to password-protected PKCS8 PEM format.
	bool GetPkcs8EncryptedPem(const wchar_t *password, CkString &outStr);
	// Writes the private key to password-protected PKCS8 PEM format.
	const wchar_t *getPkcs8EncryptedPem(const wchar_t *password);
	// Writes the private key to password-protected PKCS8 PEM format.
	const wchar_t *pkcs8EncryptedPem(const wchar_t *password);

	// Gets the private key in PKCS8 PEM format.
	bool GetPkcs8Pem(CkString &outStr);
	// Gets the private key in PKCS8 PEM format.
	const wchar_t *getPkcs8Pem(void);
	// Gets the private key in PKCS8 PEM format.
	const wchar_t *pkcs8Pem(void);

	// Gets the private key in RSA DER format.
	bool GetRsaDer(CkByteData &outData);

	// Gets the private key in RSA PEM format.
	bool GetRsaPem(CkString &outStr);
	// Gets the private key in RSA PEM format.
	const wchar_t *getRsaPem(void);
	// Gets the private key in RSA PEM format.
	const wchar_t *rsaPem(void);

	// Returns the private key in XML format. The private key is returned unencrypted
	// and the parts are base64 encoded. The XML has this structure:
	// 
	// <RSAKeyValue>
	//   <Modulus>...</Modulus>
	//   <Exponent>...</Exponent>
	//   <P>...</P>
	//   <Q>...</Q>
	//   <DP>...</DP>
	//   <DQ>...</DQ>
	//   <InverseQ>...</InverseQ>
	//   <D>...</D>
	// </RSAKeyValue>
	bool GetXml(CkString &outStr);
	// Returns the private key in XML format. The private key is returned unencrypted
	// and the parts are base64 encoded. The XML has this structure:
	// 
	// <RSAKeyValue>
	//   <Modulus>...</Modulus>
	//   <Exponent>...</Exponent>
	//   <P>...</P>
	//   <Q>...</Q>
	//   <DP>...</DP>
	//   <DQ>...</DQ>
	//   <InverseQ>...</InverseQ>
	//   <D>...</D>
	// </RSAKeyValue>
	const wchar_t *getXml(void);
	// Returns the private key in XML format. The private key is returned unencrypted
	// and the parts are base64 encoded. The XML has this structure:
	// 
	// <RSAKeyValue>
	//   <Modulus>...</Modulus>
	//   <Exponent>...</Exponent>
	//   <P>...</P>
	//   <Q>...</Q>
	//   <DP>...</DP>
	//   <DQ>...</DQ>
	//   <InverseQ>...</InverseQ>
	//   <D>...</D>
	// </RSAKeyValue>
	const wchar_t *xml(void);

	// Loads the private key from an in-memory encrypted PEM string.
	bool LoadEncryptedPem(const wchar_t *pemStr, const wchar_t *password);

	// Loads a private key from an encrypted PEM file.
	bool LoadEncryptedPemFile(const wchar_t *path, const wchar_t *password);

	// Loads the private key from an in-memory PEM string.
	bool LoadPem(const wchar_t *str);

	// Loads a private key from a PEM file.
	bool LoadPemFile(const wchar_t *path);

	// Loads a private key from in-memory PKCS8 byte data.
	bool LoadPkcs8(const CkByteData &data);

	// Loads a private key from in-memory password-protected PKCS8 byte data.
	bool LoadPkcs8Encrypted(const CkByteData &data, const wchar_t *password);

	// Loads a private key from a password-protected PKCS8 file.
	bool LoadPkcs8EncryptedFile(const wchar_t *path, const wchar_t *password);

	// Loads a private key from a PKCS8 file.
	bool LoadPkcs8File(const wchar_t *path);

#if defined(CK_CRYPTOAPI_INCLUDED)
	// Loads a private key from in-memory PVK byte data.
	bool LoadPvk(const CkByteData &data, const wchar_t *password);
#endif

#if defined(CK_CRYPTOAPI_INCLUDED)
	// Loads a private key from a PVK format file.
	bool LoadPvkFile(const wchar_t *path, const wchar_t *password);
#endif

	// Loads a private key from in-memory RSA DER byte data.
	bool LoadRsaDer(const CkByteData &data);

	// Loads a private key from an RSA DER format file.
	bool LoadRsaDerFile(const wchar_t *path);

	// Loads a private key from an in-memory XML string.
	bool LoadXml(const wchar_t *xml);

	// Loads a private key from an XML file.
	bool LoadXmlFile(const wchar_t *path);

	// Saves the private key to a password-protected PKCS8 format file.
	bool SavePkcs8EncryptedFile(const wchar_t *password, const wchar_t *path);

	// Saves the private key to a password-protected PKCS8 PEM format file.
	bool SavePkcs8EncryptedPemFile(const wchar_t *password, const wchar_t *path);

	// Saves the private key to a PKCS8 format file.
	bool SavePkcs8File(const wchar_t *path);

	// Saves the private key to a PKCS8 PEM format file.
	bool SavePkcs8PemFile(const wchar_t *path);

	// Saves the private key to a RSA DER format file.
	bool SaveRsaDerFile(const wchar_t *path);

	// Saves the private key to a RSA PEM format file.
	bool SaveRsaPemFile(const wchar_t *path);

	// Saves the private key to an XML file.
	bool SaveXmlFile(const wchar_t *path);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
