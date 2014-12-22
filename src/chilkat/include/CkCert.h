// CkCert.h: interface for the CkCert class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkCert_H
#define _CkCert_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;
class CkPrivateKey;
class CkPublicKey;
class CkDateTime;
class CkXmlCertVault;
class CkCertChain;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkCert
class CK_VISIBLE_PUBLIC CkCert  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkCert(const CkCert &);
	CkCert &operator=(const CkCert &);

    public:
	CkCert(void);
	virtual ~CkCert(void);

	static CkCert *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// Applies only when running on a Microsoft Windows operating system. If true,
	// then any method that returns a certificate will not try to also access the
	// associated private key, assuming one exists. This is useful if the certificate
	// was installed with high-security such that a private key access would trigger
	// the Windows OS to display a security warning dialog. The default value of this
	// property is false.
	bool get_AvoidWindowsPkAccess(void);
	// Applies only when running on a Microsoft Windows operating system. If true,
	// then any method that returns a certificate will not try to also access the
	// associated private key, assuming one exists. This is useful if the certificate
	// was installed with high-security such that a private key access would trigger
	// the Windows OS to display a security warning dialog. The default value of this
	// property is false.
	void put_AvoidWindowsPkAccess(bool newVal);

	// The version of the certificate (1, 2, or 3). A value of 0 indicates an error --
	// the most likely cause being that the certificate object is empty (i.e. was never
	// loaded with a certificate). Note: This is not the version of the software, it is
	// the version of the X.509 certificate object. The version of the Chilkat
	// certificate software is indicated by the Version property.
	int get_CertVersion(void);

	// (Relevant only when running on a Microsoft Windows operating system.) If the
	// HasKeyContainer property is true, then the certificate is linked to a key
	// container and this property contains the name of the associated CSP
	// (cryptographic service provider). When a certificate is linked to a key
	// container , the following properties will provide information about the key
	// container and private key: CspName, KeyContainerName, MachineKeyset, and Silent.
	void get_CspName(CkString &str);
	// (Relevant only when running on a Microsoft Windows operating system.) If the
	// HasKeyContainer property is true, then the certificate is linked to a key
	// container and this property contains the name of the associated CSP
	// (cryptographic service provider). When a certificate is linked to a key
	// container , the following properties will provide information about the key
	// container and private key: CspName, KeyContainerName, MachineKeyset, and Silent.
	const char *cspName(void);

	// Has a value of true if the certificate or any certificate in the chain of
	// authority has expired. (This information is not available when running on
	// Windows 95/98 computers.)
	bool get_Expired(void);

	// true if this certificate can be used for client authentication, otherwise
	// false.
	bool get_ForClientAuthentication(void);

	// true if this certificate can be used for code signing, otherwise false.
	bool get_ForCodeSigning(void);

	// true if this certificate can be used for sending secure email, otherwise
	// false.
	bool get_ForSecureEmail(void);

	// true if this certificate can be used for server authentication, otherwise
	// false.
	bool get_ForServerAuthentication(void);

	// true if this certificate can be used for time stamping, otherwise false.
	bool get_ForTimeStamping(void);

	// (Relevant only when running on a Microsoft Windows operating system.) Indicates
	// whether this certificate is linked to a key container. If true then the
	// certificate is linked to a key container (usually containing a private key). If
	// false, then it is not.
	// 
	// When a certificate is linked to a key container , the following properties will
	// provide information about the key container and private key: CspName,
	// KeyContainerName, MachineKeyset, and Silent.
	bool get_HasKeyContainer(void);

	// Bitflags indicating the intended usages of the certificate. The flags are:
	// Digital Signature: 0x80
	// Non-Repudiation: 0x40
	// Key Encipherment: 0x20
	// Data Encipherment: 0x10
	// Key Agreement: 0x08
	// Certificate Signing: 0x04
	// CRL Signing: 0x02
	// Encipher-Only: 0x01
	unsigned long get_IntendedKeyUsage(void);

	// true if this is the root certificate, otherwise false.
	bool get_IsRoot(void);

	// The certificate issuer's country.
	void get_IssuerC(CkString &str);
	// The certificate issuer's country.
	const char *issuerC(void);

	// The certificate issuer's common name.
	void get_IssuerCN(CkString &str);
	// The certificate issuer's common name.
	const char *issuerCN(void);

	// The issuer's full distinguished name.
	void get_IssuerDN(CkString &str);
	// The issuer's full distinguished name.
	const char *issuerDN(void);

	// The certificate issuer's email address.
	void get_IssuerE(CkString &str);
	// The certificate issuer's email address.
	const char *issuerE(void);

	// The certificate issuer's locality, which could be a city, count, township, or
	// other geographic region.
	void get_IssuerL(CkString &str);
	// The certificate issuer's locality, which could be a city, count, township, or
	// other geographic region.
	const char *issuerL(void);

	// The certificate issuer's organization, which is typically the company name.
	void get_IssuerO(CkString &str);
	// The certificate issuer's organization, which is typically the company name.
	const char *issuerO(void);

	// The certificate issuer's organizational unit, which is the unit within the
	// organization.
	void get_IssuerOU(CkString &str);
	// The certificate issuer's organizational unit, which is the unit within the
	// organization.
	const char *issuerOU(void);

	// The certificate issuer's state or province.
	void get_IssuerS(CkString &str);
	// The certificate issuer's state or province.
	const char *issuerS(void);

	// (Relevant only when running on a Microsoft Windows operating system.) If the
	// HasKeyContainer property is true, then the certificate is linked to a key
	// container and this property contains the name of the key container.
	// 
	// When a certificate is linked to a key container , the following properties will
	// provide information about the key container and private key: CspName,
	// KeyContainerName, MachineKeyset, and Silent.
	void get_KeyContainerName(CkString &str);
	// (Relevant only when running on a Microsoft Windows operating system.) If the
	// HasKeyContainer property is true, then the certificate is linked to a key
	// container and this property contains the name of the key container.
	// 
	// When a certificate is linked to a key container , the following properties will
	// provide information about the key container and private key: CspName,
	// KeyContainerName, MachineKeyset, and Silent.
	const char *keyContainerName(void);

	// (Relevant only when running on a Microsoft Windows operating system.) If the
	// HasKeyContainer property is true, then the certificate is linked to a key
	// container and this property indicates whether the key container is in the
	// machine's keyset or in the keyset specific to the logged on user's account. If
	// true, the key container is within the machine keyset. If false, it's in the
	// user's keyset.
	// 
	// When a certificate is linked to a key container , the following properties will
	// provide information about the key container and private key: CspName,
	// KeyContainerName, MachineKeyset, and Silent.
	bool get_MachineKeyset(void);

	// If present in the certificate's extensions, returns the OCSP URL of the
	// certificate. (The Online Certificate Status Protocol (OCSP) is an Internet
	// protocol used for obtaining the revocation status of an X.509 digital
	// certificate.)
	void get_OcspUrl(CkString &str);
	// If present in the certificate's extensions, returns the OCSP URL of the
	// certificate. (The Online Certificate Status Protocol (OCSP) is an Internet
	// protocol used for obtaining the revocation status of an X.509 digital
	// certificate.)
	const char *ocspUrl(void);

	// (Relevant only when running on a Microsoft Windows operating system.) Indicates
	// whether the private key was installed with security settings that allow it to be
	// re-exported.
	bool get_PrivateKeyExportable(void);

	// true if the certificate or any certificate in the chain of authority has been
	// revoked. This information is not available when running on Windows 95/98
	// computers. Note: If this property is false, it could mean that it was not able
	// to check the revocation status. Because of this uncertainty, a CheckRevoked
	// method has been added. It returns an integer indicating one of three possible
	// states: 1 (revoked) , 0 (not revoked), -1 (unable to check revocation status).
	bool get_Revoked(void);

	// The RFC-822 name of the certificate. (Also known as the Subject Alternative
	// Name.)
	// 
	// If the certificate contains a list of Subject Alternative Names, such as a list
	// of host names to be protected by a single SSL certificate, then this property
	// will contain the comma separated list of names.
	// 
	void get_Rfc822Name(CkString &str);
	// The RFC-822 name of the certificate. (Also known as the Subject Alternative
	// Name.)
	// 
	// If the certificate contains a list of Subject Alternative Names, such as a list
	// of host names to be protected by a single SSL certificate, then this property
	// will contain the comma separated list of names.
	// 
	const char *rfc822Name(void);

	// true if this is a self-signed certificate, otherwise false.
	bool get_SelfSigned(void);

	// The certificate's serial number as a hexidecimal string.
	void get_SerialNumber(CkString &str);
	// The certificate's serial number as a hexidecimal string.
	const char *serialNumber(void);

	// Hexidecimal string of the SHA-1 thumbprint for the certificate.
	void get_Sha1Thumbprint(CkString &str);
	// Hexidecimal string of the SHA-1 thumbprint for the certificate.
	const char *sha1Thumbprint(void);

	// Returns true if the certificate and all certificates in the chain of authority
	// have valid signatures, otherwise returns false.
	bool get_SignatureVerified(void);

	// (Relevant only when running on a Microsoft Windows operating system.) If the
	// HasKeyContainer property is true, then the certificate is linked to a key
	// container and this property indicates whether accessing the private key will
	// cause the operating system to launch an interactive warning dialog. If false a
	// warning dialog will be displayed. If true then private key accesses are
	// silent.
	// 
	// When a certificate is linked to a key container , the following properties will
	// provide information about the key container and private key: CspName,
	// KeyContainerName, MachineKeyset, and Silent.
	bool get_Silent(void);

	// The certificate subject's country.
	void get_SubjectC(CkString &str);
	// The certificate subject's country.
	const char *subjectC(void);

	// The certificate subject's common name.
	void get_SubjectCN(CkString &str);
	// The certificate subject's common name.
	const char *subjectCN(void);

	// The certificate subject's full distinguished name.
	void get_SubjectDN(CkString &str);
	// The certificate subject's full distinguished name.
	const char *subjectDN(void);

	// The certificate subject's email address.
	void get_SubjectE(CkString &str);
	// The certificate subject's email address.
	const char *subjectE(void);

	// The certificate subject's locality, which could be a city, count, township, or
	// other geographic region.
	void get_SubjectL(CkString &str);
	// The certificate subject's locality, which could be a city, count, township, or
	// other geographic region.
	const char *subjectL(void);

	// The certificate subject's organization, which is typically the company name.
	void get_SubjectO(CkString &str);
	// The certificate subject's organization, which is typically the company name.
	const char *subjectO(void);

	// The certificate subject's organizational unit, which is the unit within the
	// organization.
	void get_SubjectOU(CkString &str);
	// The certificate subject's organizational unit, which is the unit within the
	// organization.
	const char *subjectOU(void);

	// The certificate subject's state or province.
	void get_SubjectS(CkString &str);
	// The certificate subject's state or province.
	const char *subjectS(void);

	// Returns true if the certificate has a trusted root authority, otherwise
	// returns false. (This property only works for the Windows operating system. On
	// other systems, the property always returns false.)
	bool get_TrustedRoot(void);

	// The date this certificate becomes (or became) valid.
	void get_ValidFrom(SYSTEMTIME &outSysTime);

	// The date (in RFC822 string format) that this certificate becomes (or became)
	// valid.
	void get_ValidFromStr(CkString &str);
	// The date (in RFC822 string format) that this certificate becomes (or became)
	// valid.
	const char *validFromStr(void);

	// The date this certificate becomes (or became) invalid.
	void get_ValidTo(SYSTEMTIME &outSysTime);

	// The date (in RFC822 string format) that this certificate becomes (or became)
	// invalid.
	void get_ValidToStr(CkString &str);
	// The date (in RFC822 string format) that this certificate becomes (or became)
	// invalid.
	const char *validToStr(void);

	// The subject key identifier of the certificate in base64 string format. This is
	// only present if the certificate contains the extension OID 2.5.29.14.
	void get_SubjectKeyId(CkString &str);
	// The subject key identifier of the certificate in base64 string format. This is
	// only present if the certificate contains the extension OID 2.5.29.14.
	const char *subjectKeyId(void);

	// The authority key identifier of the certificate in base64 string format. This is
	// only present if the certificate contains the extension OID 2.5.29.35.
	void get_AuthorityKeyId(CkString &str);
	// The authority key identifier of the certificate in base64 string format. This is
	// only present if the certificate contains the extension OID 2.5.29.35.
	const char *authorityKeyId(void);



	// ----------------------
	// Methods
	// ----------------------
	// Returns 1 if the certificate has been revoked, 0 if not revoked, and -1 if
	// unable to check the revocation status.
	int CheckRevoked(void);

	// Exports the digital certificate to ASN.1 DER format.
	bool ExportCertDer(CkByteData &outData);

	// Exports the digital certificate to ASN.1 DER format binary file.
	bool ExportCertDerFile(const char *path);

	// Exports the digital certificate to an unencrypted PEM formatted string.
	bool ExportCertPem(CkString &outStr);
	// Exports the digital certificate to an unencrypted PEM formatted string.
	const char *exportCertPem(void);

	// Exports the digital certificate to an unencrypted PEM formatted file.
	bool ExportCertPemFile(const char *path);

	// Exports a certificate to an XML format where the XML tags are the names of the
	// ASN.1 objects that compose the X.509 certificate. Binary data is either hex or
	// base64 encoded. (The binary data for a "bits" ASN.1 tag is hex encoded, whereas
	// for all other ASN.1 tags, such as "octets", it is base64.)
	bool ExportCertXml(CkString &outStr);
	// Exports a certificate to an XML format where the XML tags are the names of the
	// ASN.1 objects that compose the X.509 certificate. Binary data is either hex or
	// base64 encoded. (The binary data for a "bits" ASN.1 tag is hex encoded, whereas
	// for all other ASN.1 tags, such as "octets", it is base64.)
	const char *exportCertXml(void);

	// Exports the certificate's private key.
	// The caller is responsible for deleting the object returned by this method.
	CkPrivateKey *ExportPrivateKey(void);

	// Exports the certificate's public key.
	// The caller is responsible for deleting the object returned by this method.
	CkPublicKey *ExportPublicKey(void);

	// Exports the certificate and private key (if available) to a PFX (.pfx or .p12)
	// file. The output PFX is secured using the  pfxPassword. If  bIncludeCertChain is true, then the
	// certificates in the chain of authority are also included in the PFX output file.
	bool ExportToPfxFile(const char *pfxFilename, const char *password, bool bIncludeChain);

	// Finds and returns the issuer certificate. If the certificate is a root or
	// self-issued, then the certificate returned is a copy of the caller certificate.
	// (The IsRoot property can be check to see if the certificate is a root (or
	// self-issued) certificate.)
	// The caller is responsible for deleting the object returned by this method.
	CkCert *FindIssuer(void);

	// Returns a base64 encoded string representation of the certificate's binary DER
	// format, which can be passed to SetFromEncoded to recreate the certificate
	// object.
	bool GetEncoded(CkString &outStr);
	// Returns a base64 encoded string representation of the certificate's binary DER
	// format, which can be passed to SetFromEncoded to recreate the certificate
	// object.
	const char *getEncoded(void);
	// Returns a base64 encoded string representation of the certificate's binary DER
	// format, which can be passed to SetFromEncoded to recreate the certificate
	// object.
	const char *encoded(void);

	// Exports the certificate's private key to a PEM string (if the private key is
	// available).
	bool GetPrivateKeyPem(CkString &outStr);
	// Exports the certificate's private key to a PEM string (if the private key is
	// available).
	const char *getPrivateKeyPem(void);
	// Exports the certificate's private key to a PEM string (if the private key is
	// available).
	const char *privateKeyPem(void);

	// Returns the date/time this certificate becomes (or became) valid.
	// The caller is responsible for deleting the object returned by this method.
	CkDateTime *GetValidFromDt(void);

	// Returns the date/time this certificate becomes (or became) invalid.
	// The caller is responsible for deleting the object returned by this method.
	CkDateTime *GetValidToDt(void);

	// Returns true if the private key is installed on the local system for the
	// certificate.
	bool HasPrivateKey(void);

#if defined(CK_CRYPTOAPI_INCLUDED)
	// (Relevant only when running on a Microsoft Windows operating system.) Associates
	// a private key with a certificate. The private key is specified by providing the
	// name of the key container where it can be found. The 2nd argument indicates
	// whether the key container is from the machine-wide keyset (true), or from the
	// keyset specific to the logged-on user (false). Private keys can be imported
	// into a key container by calling the KeyContainer's ImportPrivateKey method. Once
	// a certificate has been linked, the private key is available for creating digital
	// signatures or decrypting. Note: Certificates imported from a PFX or from a
	// Certificate Authority will already be "linked" and it is not necessary to call
	// this method.
	bool LinkPrivateKey(const char *keyContainerName, bool bMachineKeyset, bool bForSigning);
#endif

	// (Relevant only when running on a Microsoft Windows operating system.) Searches
	// the Windows Local Machine and Current User registry-based certificate stores for
	// a certificate having the common name specified. If found, the certificate is
	// loaded and ready for use.
	bool LoadByCommonName(const char *cn);

	// (Relevant only when running on a Microsoft Windows operating system.) Searches
	// the Windows Local Machine and Current User registry-based certificate stores for
	// a certificate containing the email address specified. If found, the certificate
	// is loaded and ready for use.
	bool LoadByEmailAddress(const char *emailAddress);

	// (Relevant only when running on a Microsoft Windows operating system.) Searches
	// the Windows Local Machine and Current User registry-based certificate stores for
	// a certificate matching the issuerCN and having an issuer matching the  serialNumber. If
	// found, the certificate is loaded and ready for use.
	bool LoadByIssuerAndSerialNumber(const char *issuerCN, const char *serialNum);

	// Loads an ASN.1 or DER encoded certificate represented in a Base64 string.
	bool LoadFromBase64(const char *encodedCert);

	// Loads an X.509 certificate from ASN.1 DER encoded bytes.
	bool LoadFromBinary(const CkByteData &data);

#if !defined(CHILKAT_MONO)
	// The same as LoadFromBinary, but instead of using a CkByteData object, the
	// pointer to the byte data and length (in number of bytes) are specified directly
	// in the method arguments.
	bool LoadFromBinary2(const unsigned char *pByteData, unsigned long szByteData);
#endif

	// Loads a certificate from a .cer, .crt, .p7b, or .pem file. This method accepts
	// certificates from files in any of the following formats:
	// 1. DER encoded binary X.509 (.CER)
	// 2. Base-64 encoded X.509 (.CER)
	// 3. Cryptographic Message Syntax Standard - PKCS #7 Certificates (.P7B)
	// 4. PEM format
	// This method decodes the certificate based on the contents if finds within the
	// file, and not based on the file extension. If your certificate is in a file
	// having a different extension, try loading it using this method before assuming
	// it won't work. This method does not load .p12 or .pfx (PKCS #12) files.
	bool LoadFromFile(const char *path);

	// Loads a PFX from an in-memory image of a PFX file. Note: If the PFX contains
	// multiple certificates, the 1st certificate in the PFX is loaded.
	bool LoadPfxData(const CkByteData &pfxData, const char *password);

#if !defined(CHILKAT_MONO)
	// Loads a PFX from an in-memory image of a PFX file. Note: If the PFX contains
	// multiple certificates, the 1st certificate in the PFX is loaded.
	bool LoadPfxData2(const unsigned char *pByteData, unsigned long szByteData, const char *password);
#endif

	// Loads a PFX file. Note: If the PFX contains multiple certificates, the 1st
	// certificate in the PFX is loaded.
	bool LoadPfxFile(const char *pfxPath, const char *password);

	// Converts a PEM file to a DER file.
	bool PemFileToDerFile(const char *fromPath, const char *toPath);

	// Saves a certificate object to a .cer file.
	bool SaveToFile(const char *path);

	// Initializes the certificate object from a base64 encoded string representation
	// of the certificate's binary DER format.
	bool SetFromEncoded(const char *encodedCert);

	// Used to associate a private key with the certificate for subsequent (PKCS7)
	// signature creation or decryption.
	bool SetPrivateKey(CkPrivateKey &privKey);

	// Same as SetPrivateKey, but the key is provided in unencrypted PEM format. (Note:
	// The privKeyPem is not a file path, it is the actual PEM text.)
	bool SetPrivateKeyPem(const char *privKeyPem);

	// Exports the certificate and private key (if available) to an in-memory PFX
	// image. The ARG1 is what will be required to access the PFX contents at a later
	// time. If ARG2 is true, then the certificates in the chain of authority are
	// also included in the PFX.
	bool ExportToPfxData(const char *password, bool includeCertChain, CkByteData &outBytes);

	// Adds an XML certificate vault to the object's internal list of sources to be
	// searched for certificates for help in building certificate chains and verifying
	// the certificate signature to the trusted root.
	bool UseCertVault(CkXmlCertVault &vault);

	// Returns a certficate chain object containing all the certificates (including
	// this one), in the chain of authentication to the trusted root. If this
	// certificate object was loaded from a PFX, then the certiicates contained in the
	// PFX are automatically available for building the certificate chain. The
	// UseCertVault method can be called to provide additional certificates that might
	// be required to build the cert chain. Finally, the TrustedRoots object can be
	// used to provide a way of making trusted root certificates available.
	// 
	// On Windows systems, the registry-based certificate stores are automatically
	// consulted if needed to locate intermediate or root certificates in the chain.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkCertChain *GetCertChain(void);

	// Verifies the certificate signature, as well as the signatures of all
	// certificates in the chain of authentication to the trusted root. Returns true
	// if all signatures are verified to the trusted root. Otherwise returns false.
	bool VerifySignature(void);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
