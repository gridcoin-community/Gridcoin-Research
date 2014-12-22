// CkXmlCertVaultW.h: interface for the CkXmlCertVaultW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkXmlCertVaultW_H
#define _CkXmlCertVaultW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkByteData;
class CkCertW;
class CkPfxW;
class CkCertChainW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkXmlCertVaultW
class CK_VISIBLE_PUBLIC CkXmlCertVaultW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkXmlCertVaultW(const CkXmlCertVaultW &);
	CkXmlCertVaultW &operator=(const CkXmlCertVaultW &);

    public:
	CkXmlCertVaultW(void);
	virtual ~CkXmlCertVaultW(void);

	static CkXmlCertVaultW *createNew(void);
	

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The master password for the vault. Certificates are stored unencrypted, but
	// private keys are stored 256-bit AES encrypted using the individual PFX
	// passwords. The PFX passwords are stored 256-bit AES encrypted using the master
	// password. The master password is required to acces any of the private keys
	// stored within the XML vault.
	void get_MasterPassword(CkString &str);
	// The master password for the vault. Certificates are stored unencrypted, but
	// private keys are stored 256-bit AES encrypted using the individual PFX
	// passwords. The PFX passwords are stored 256-bit AES encrypted using the master
	// password. The master password is required to acces any of the private keys
	// stored within the XML vault.
	const wchar_t *masterPassword(void);
	// The master password for the vault. Certificates are stored unencrypted, but
	// private keys are stored 256-bit AES encrypted using the individual PFX
	// passwords. The PFX passwords are stored 256-bit AES encrypted using the master
	// password. The master password is required to acces any of the private keys
	// stored within the XML vault.
	void put_MasterPassword(const wchar_t *newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Adds a PFX file to the XML vault.
	bool AddPfxFile(const wchar_t *path, const wchar_t *password);

	// Adds a certificate to the XML vault.
	bool AddCertFile(const wchar_t *path);

	// Adds the contents of a PEM file to the XML vault. The PEM file may be encrypted,
	// and it may contain one or more certificates and/or private keys. The password is
	// optional and only required if the PEM file contains encrypted content that
	// requires a password.
	bool AddPemFile(const wchar_t *path, const wchar_t *password);

	// Saves the contents to an XML file.
	bool SaveXml(const wchar_t *path);

	// Loads from an XML string.
	bool LoadXml(const wchar_t *xml);

	// Loads from an XML file.
	bool LoadXmlFile(const wchar_t *path);

	// Returns the contents of the cert vault as an XML string.
	bool GetXml(CkString &outStr);
	// Returns the contents of the cert vault as an XML string.
	const wchar_t *getXml(void);
	// Returns the contents of the cert vault as an XML string.
	const wchar_t *xml(void);

	// Adds a certificate to the XML vault from any binary format, such as DER.
	bool AddCertBinary(const CkByteData &certBytes);

	// Adds a PFX to the XML vault where PFX is passed directly from in-memory binary
	// bytes.
	bool AddPfxBinary(const CkByteData &pfxBytes, const wchar_t *password);

	// Adds a certificate from any string representation format such as PEM.
	bool AddCertString(const wchar_t *certData);

	// Adds a PFX to the XML vault where PFX is passed directly from encoded bytes
	// (such as Base64, Hex, etc.). The encoding is indicated by ARG2.
	bool AddPfxEncoded(const wchar_t *encodedBytes, const wchar_t *encoding, const wchar_t *password);

	// Adds a certificate to the XML vault where certificate is passed directly from
	// encoded bytes (such as Base64, Hex, etc.). The encoding is indicated by ARG2.
	bool AddCertEncoded(const wchar_t *encodedBytes, const wchar_t *encoding);

	// Adds a certificate to the XML vault.
	bool AddCert(CkCertW &cert);

	// Adds a PFX (PKCS12) to the XML vault.
	bool AddPfx(CkPfxW &pfx);

	// Adds a chain of certificates to the XML vault.
	bool AddCertChain(CkCertChainW &certChain);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
