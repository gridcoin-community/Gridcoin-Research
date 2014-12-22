// CkPrivateKey.h: interface for the CkPrivateKey class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkPrivateKey_H
#define _CkPrivateKey_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkPrivateKey
class CK_VISIBLE_PUBLIC CkPrivateKey  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkPrivateKey(const CkPrivateKey &);
	CkPrivateKey &operator=(const CkPrivateKey &);

    public:
	CkPrivateKey(void);
	virtual ~CkPrivateKey(void);

	static CkPrivateKey *createNew(void);
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
	bool GetPkcs8Encrypted(const char *password, CkByteData &outBytes);

	// Writes the private key to password-protected PKCS8 PEM format.
	bool GetPkcs8EncryptedPem(const char *password, CkString &outStr);
	// Writes the private key to password-protected PKCS8 PEM format.
	const char *getPkcs8EncryptedPem(const char *password);
	// Writes the private key to password-protected PKCS8 PEM format.
	const char *pkcs8EncryptedPem(const char *password);

	// Gets the private key in PKCS8 PEM format.
	bool GetPkcs8Pem(CkString &outStr);
	// Gets the private key in PKCS8 PEM format.
	const char *getPkcs8Pem(void);
	// Gets the private key in PKCS8 PEM format.
	const char *pkcs8Pem(void);

	// Gets the private key in RSA DER format.
	bool GetRsaDer(CkByteData &outData);

	// Gets the private key in RSA PEM format.
	bool GetRsaPem(CkString &outStr);
	// Gets the private key in RSA PEM format.
	const char *getRsaPem(void);
	// Gets the private key in RSA PEM format.
	const char *rsaPem(void);

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
	const char *getXml(void);
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
	const char *xml(void);

	// Loads the private key from an in-memory encrypted PEM string.
	bool LoadEncryptedPem(const char *pemStr, const char *password);

	// Loads a private key from an encrypted PEM file.
	bool LoadEncryptedPemFile(const char *path, const char *password);

	// Loads the private key from an in-memory PEM string.
	bool LoadPem(const char *str);

	// Loads a private key from a PEM file.
	bool LoadPemFile(const char *path);

	// Loads a private key from in-memory PKCS8 byte data.
	bool LoadPkcs8(const CkByteData &data);

	// Loads a private key from in-memory password-protected PKCS8 byte data.
	bool LoadPkcs8Encrypted(const CkByteData &data, const char *password);

	// Loads a private key from a password-protected PKCS8 file.
	bool LoadPkcs8EncryptedFile(const char *path, const char *password);

	// Loads a private key from a PKCS8 file.
	bool LoadPkcs8File(const char *path);

#if defined(CK_CRYPTOAPI_INCLUDED)
	// Loads a private key from in-memory PVK byte data.
	bool LoadPvk(const CkByteData &data, const char *password);
#endif

#if defined(CK_CRYPTOAPI_INCLUDED)
	// Loads a private key from a PVK format file.
	bool LoadPvkFile(const char *path, const char *password);
#endif

	// Loads a private key from in-memory RSA DER byte data.
	bool LoadRsaDer(const CkByteData &data);

	// Loads a private key from an RSA DER format file.
	bool LoadRsaDerFile(const char *path);

	// Loads a private key from an in-memory XML string.
	bool LoadXml(const char *xml);

	// Loads a private key from an XML file.
	bool LoadXmlFile(const char *path);

	// Saves the private key to a password-protected PKCS8 format file.
	bool SavePkcs8EncryptedFile(const char *password, const char *path);

	// Saves the private key to a password-protected PKCS8 PEM format file.
	bool SavePkcs8EncryptedPemFile(const char *password, const char *path);

	// Saves the private key to a PKCS8 format file.
	bool SavePkcs8File(const char *path);

	// Saves the private key to a PKCS8 PEM format file.
	bool SavePkcs8PemFile(const char *path);

	// Saves the private key to a RSA DER format file.
	bool SaveRsaDerFile(const char *path);

	// Saves the private key to a RSA PEM format file.
	bool SaveRsaPemFile(const char *path);

	// Saves the private key to an XML file.
	bool SaveXmlFile(const char *path);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
