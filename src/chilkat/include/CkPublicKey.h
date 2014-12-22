// CkPublicKey.h: interface for the CkPublicKey class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkPublicKey_H
#define _CkPublicKey_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkPublicKey
class CK_VISIBLE_PUBLIC CkPublicKey  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkPublicKey(const CkPublicKey &);
	CkPublicKey &operator=(const CkPublicKey &);

    public:
	CkPublicKey(void);
	virtual ~CkPublicKey(void);

	static CkPublicKey *createNew(void);
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
	// Gets the public key in OpenSSL DER format.
	bool GetOpenSslDer(CkByteData &outData);

	// Gets the public key in OpenSSL PEM format.
	bool GetOpenSslPem(CkString &outStr);
	// Gets the public key in OpenSSL PEM format.
	const char *getOpenSslPem(void);
	// Gets the public key in OpenSSL PEM format.
	const char *openSslPem(void);

	// Gets the public key in RSA DER format.
	bool GetRsaDer(CkByteData &outData);

	// Gets the public key in XML format. The XML has this format where the key parts
	// are base64 encoded:
	// 
	// <RSAKeyValue>
	//   <Modulus>...</Modulus>
	//   <Exponent>...</Exponent>
	// </RSAKeyValue>
	bool GetXml(CkString &outStr);
	// Gets the public key in XML format. The XML has this format where the key parts
	// are base64 encoded:
	// 
	// <RSAKeyValue>
	//   <Modulus>...</Modulus>
	//   <Exponent>...</Exponent>
	// </RSAKeyValue>
	const char *getXml(void);
	// Gets the public key in XML format. The XML has this format where the key parts
	// are base64 encoded:
	// 
	// <RSAKeyValue>
	//   <Modulus>...</Modulus>
	//   <Exponent>...</Exponent>
	// </RSAKeyValue>
	const char *xml(void);

	// Loads a public key from in-memory OpenSSL DER formatted byte data.
	bool LoadOpenSslDer(const CkByteData &data);

	// Loads a public key from an OpenSSL DER format file.
	bool LoadOpenSslDerFile(const char *path);

	// Loads a public key from an OpenSSL PEM string.
	bool LoadOpenSslPem(const char *str);

	// Loads a public key from an OpenSSL PEM file.
	bool LoadOpenSslPemFile(const char *path);

	// Loads an RSA public key from PKCS#1 PEM format.
	bool LoadPkcs1Pem(const char *str);

	// Loads a public key from in-memory RSA DER formatted byte data.
	bool LoadRsaDer(const CkByteData &data);

	// Loads a public key from an RSA DER formatted file.
	bool LoadRsaDerFile(const char *path);

	// Loads a public key from an XML string.
	bool LoadXml(const char *xml);

	// Loads a public key from an XML file.
	bool LoadXmlFile(const char *path);

	// Saves the public key to an OpenSSL DER format file.
	bool SaveOpenSslDerFile(const char *path);

	// Saves the public key to an OpenSSL PEM format file.
	bool SaveOpenSslPemFile(const char *path);

	// Saves the public key to an RSA DER format file.
	bool SaveRsaDerFile(const char *path);

	// Saves the public key to an XML file.
	bool SaveXmlFile(const char *path);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
