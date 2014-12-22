// CkDkim.h: interface for the CkDkim class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkDkim_H
#define _CkDkim_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;
class CkBaseProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkDkim
class CK_VISIBLE_PUBLIC CkDkim  : public CkMultiByteBase
{
    private:
	CkBaseProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkDkim(const CkDkim &);
	CkDkim &operator=(const CkDkim &);

    public:
	CkDkim(void);
	virtual ~CkDkim(void);

	static CkDkim *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	CkBaseProgress *get_EventCallbackObject(void) const;
	void put_EventCallbackObject(CkBaseProgress *progress);


	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The signing algorithm to be used in creating the DKIM-Signature. Possible values
	// are "rsa-sha256" and "rsa-sha1". The default value is "rsa-sha256".
	void get_DkimAlg(CkString &str);
	// The signing algorithm to be used in creating the DKIM-Signature. Possible values
	// are "rsa-sha256" and "rsa-sha1". The default value is "rsa-sha256".
	const char *dkimAlg(void);
	// The signing algorithm to be used in creating the DKIM-Signature. Possible values
	// are "rsa-sha256" and "rsa-sha1". The default value is "rsa-sha256".
	void put_DkimAlg(const char *newVal);

	// Optional body length count to set a maximum number of body bytes to be hashed
	// when creating the DKIM-Signature field. The default value is 0, indicating that
	// the entire body should be hashed.
	int get_DkimBodyLengthCount(void);
	// Optional body length count to set a maximum number of body bytes to be hashed
	// when creating the DKIM-Signature field. The default value is 0, indicating that
	// the entire body should be hashed.
	void put_DkimBodyLengthCount(int newVal);

	// Canonicalization algorithm to be used for both header and body when creating the
	// DKIM-Signature. Possible values are "simple" and "relaxed". The default value is
	// "relaxed".
	void get_DkimCanon(CkString &str);
	// Canonicalization algorithm to be used for both header and body when creating the
	// DKIM-Signature. Possible values are "simple" and "relaxed". The default value is
	// "relaxed".
	const char *dkimCanon(void);
	// Canonicalization algorithm to be used for both header and body when creating the
	// DKIM-Signature. Possible values are "simple" and "relaxed". The default value is
	// "relaxed".
	void put_DkimCanon(const char *newVal);

	// The domain name of the signing domain when creating the DKIM-Signature.
	void get_DkimDomain(CkString &str);
	// The domain name of the signing domain when creating the DKIM-Signature.
	const char *dkimDomain(void);
	// The domain name of the signing domain when creating the DKIM-Signature.
	void put_DkimDomain(const char *newVal);

	// A colon-separated list of header field names that identify headers presented to
	// the signing algorithm when creating the DKIM-Signature. The default value is:
	// "mime-version:date:message-id:subject:from:to:content-type".
	void get_DkimHeaders(CkString &str);
	// A colon-separated list of header field names that identify headers presented to
	// the signing algorithm when creating the DKIM-Signature. The default value is:
	// "mime-version:date:message-id:subject:from:to:content-type".
	const char *dkimHeaders(void);
	// A colon-separated list of header field names that identify headers presented to
	// the signing algorithm when creating the DKIM-Signature. The default value is:
	// "mime-version:date:message-id:subject:from:to:content-type".
	void put_DkimHeaders(const char *newVal);

	// The selector to be used to form the DNS query for the public key. This property
	// applies to creating a DKIM-Signature. For example, if the selector is
	// "reykjavik" and the domain is "example-code.com", then the DNS query would be
	// for "reykjavik._domainkey.example-code.com".
	void get_DkimSelector(CkString &str);
	// The selector to be used to form the DNS query for the public key. This property
	// applies to creating a DKIM-Signature. For example, if the selector is
	// "reykjavik" and the domain is "example-code.com", then the DNS query would be
	// for "reykjavik._domainkey.example-code.com".
	const char *dkimSelector(void);
	// The selector to be used to form the DNS query for the public key. This property
	// applies to creating a DKIM-Signature. For example, if the selector is
	// "reykjavik" and the domain is "example-code.com", then the DNS query would be
	// for "reykjavik._domainkey.example-code.com".
	void put_DkimSelector(const char *newVal);

	// The signing algorithm to be used in creating the DomainKey-Signature. The only
	// possible value is "rsa-sha1". The default value is "rsa-sha1".
	void get_DomainKeyAlg(CkString &str);
	// The signing algorithm to be used in creating the DomainKey-Signature. The only
	// possible value is "rsa-sha1". The default value is "rsa-sha1".
	const char *domainKeyAlg(void);
	// The signing algorithm to be used in creating the DomainKey-Signature. The only
	// possible value is "rsa-sha1". The default value is "rsa-sha1".
	void put_DomainKeyAlg(const char *newVal);

	// Canonicalization algorithm to be used for when creating the DomainKey-Signature.
	// Possible values are "simple" and "nofws". The default value is "nofws".
	void get_DomainKeyCanon(CkString &str);
	// Canonicalization algorithm to be used for when creating the DomainKey-Signature.
	// Possible values are "simple" and "nofws". The default value is "nofws".
	const char *domainKeyCanon(void);
	// Canonicalization algorithm to be used for when creating the DomainKey-Signature.
	// Possible values are "simple" and "nofws". The default value is "nofws".
	void put_DomainKeyCanon(const char *newVal);

	// The domain name of the signing domain when creating the DomainKey-Signature.
	void get_DomainKeyDomain(CkString &str);
	// The domain name of the signing domain when creating the DomainKey-Signature.
	const char *domainKeyDomain(void);
	// The domain name of the signing domain when creating the DomainKey-Signature.
	void put_DomainKeyDomain(const char *newVal);

	// A colon-separated list of header field names that identify headers presented to
	// the signing algorithm when creating the DomainKey-Signature. The default value
	// is: "mime-version:date:message-id:subject:from:to:content-type".
	void get_DomainKeyHeaders(CkString &str);
	// A colon-separated list of header field names that identify headers presented to
	// the signing algorithm when creating the DomainKey-Signature. The default value
	// is: "mime-version:date:message-id:subject:from:to:content-type".
	const char *domainKeyHeaders(void);
	// A colon-separated list of header field names that identify headers presented to
	// the signing algorithm when creating the DomainKey-Signature. The default value
	// is: "mime-version:date:message-id:subject:from:to:content-type".
	void put_DomainKeyHeaders(const char *newVal);

	// The selector to be used to form the DNS query for the public key. This property
	// applies to creating a DomainKey-Signature. For example, if the selector is
	// "reykjavik" and the domain is "example-code.com", then the DNS query would be
	// for "reykjavik._domainkey.example-code.com".
	void get_DomainKeySelector(CkString &str);
	// The selector to be used to form the DNS query for the public key. This property
	// applies to creating a DomainKey-Signature. For example, if the selector is
	// "reykjavik" and the domain is "example-code.com", then the DNS query would be
	// for "reykjavik._domainkey.example-code.com".
	const char *domainKeySelector(void);
	// The selector to be used to form the DNS query for the public key. This property
	// applies to creating a DomainKey-Signature. For example, if the selector is
	// "reykjavik" and the domain is "example-code.com", then the DNS query would be
	// for "reykjavik._domainkey.example-code.com".
	void put_DomainKeySelector(const char *newVal);

	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any method call prior to
	// completion. If HeartbeatMs is 0 (the default), no AbortCheck event callbacks
	// will fire.
	int get_HeartbeatMs(void);
	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any method call prior to
	// completion. If HeartbeatMs is 0 (the default), no AbortCheck event callbacks
	// will fire.
	void put_HeartbeatMs(int newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Constructs and prepends a DKIM-Signature header to the MIME. Prior to calling
	// this method, your program must set both the DkimDomain and DkimSelector
	// properties, and it must load a private key by calling LoadDkimPk or
	// LoadDkimPkFile.
	bool AddDkimSignature(const CkByteData &mimeIn, CkByteData &outBytes);

	// Constructs and prepends a DomainKey-Signature header to the MIME. Prior to
	// calling this method, your program must set both the DomainKeyDomain and
	// DomainKeySelector properties, and it must load a private key by calling
	// LoadDomainKeyPk or LoadDomainKeyPkFile.
	bool AddDomainKeySignature(const CkByteData &mimeIn, CkByteData &outBytes);

	// Loads an RSA private key to be used for creating a DKIM-Signature. Any valid RSA
	// private key format that is not binary, such as PEM or XML, may be passed to this
	// method. This method will automatically determine the format and parse it
	// correctly. A password is only required if key is encrypted, such as for
	// encrypted PEM.
	bool LoadDkimPk(const char *privateKey, const char *optionalPassword);

	// Loads an RSA private key to be used for creating a DKIM-Signature. Any valid RSA
	// private key binary format, such as DER, may be passed to this method. This
	// method will automatically determine the format and parse it correctly. A
	// password is only required if key is encrypted.
	bool LoadDkimPkBytes(const CkByteData &privateKeyDer, const char *optionalPassword);

	// Loads an RSA private key file to be used for creating a DKIM-Signature. The file
	// name contains any valid RSA private key format, such as DER, PKCS8, PEM, XML,
	// etc. This method will automatically determine the format and parse it correctly.
	// A password is only required if key is encrypted, such as for encrypted PEM or
	// encrypted PKCS8.
	bool LoadDkimPkFile(const char *privateKeyFilepath, const char *optionalPassword);

	// Loads an RSA private key to be used for creating a DomainKey-Signature. Any
	// valid RSA private key format that is not binary, such as PEM or XML, may be
	// passed to this method. This method will automatically determine the format and
	// parse it correctly. A password is only required if key is encrypted, such as for
	// encrypted PEM.
	bool LoadDomainKeyPk(const char *privateKey, const char *optionalPassword);

	// Loads an RSA private key to be used for creating a DomainKey-Signature. Any
	// valid RSA private key binary format, such as DER, may be passed to this method.
	// This method will automatically determine the format and parse it correctly. A
	// password is only required if key is encrypted.
	bool LoadDomainKeyPkBytes(const CkByteData &privateKeyDer, const char *optionalPassword);

	// Loads an RSA private key file to be used for creating a DomainKey-Signature. The
	// file name contains any valid RSA private key format, such as DER, PKCS8, PEM,
	// XML, etc. This method will automatically determine the format and parse it
	// correctly. A password is only required if key is encrypted, such as for
	// encrypted PEM or encrypted PKCS8.
	bool LoadDomainKeyPkFile(const char *privateKeyFilepath, const char *optionalPassword);

	// Caches a public key to be used for verifying DKIM and DomainKey signatures for a
	// given selector and domain. The  publicKey is a string containing an RSA public key in
	// any text format, such as XML, PEM, etc. This method will automatically detect
	// the format and load the public key correctly. This method is useful for testing
	// DKIM and DomainKey verification when your public key has not yet been installed
	// in DNS.
	bool LoadPublicKey(const char *selector, const char *domain, const char *publicKey);

	// Caches a public key to be used for verifying DKIM and DomainKey signatures for a
	// given selector and domain. The  publicKeyFilepath is a filepath of an RSA public key in any
	// format. This method will automatically detect the format and load the public key
	// correctly. This method is useful for testing DKIM and DomainKey verification
	// when your public key has not yet been installed in DNS.
	bool LoadPublicKeyFile(const char *selector, const char *domain, const char *publicKeyFilepath);

	// Returns the number of DKIM-Signature header fields found in mimeData.
	int NumDkimSignatures(const CkByteData &mimeData);

	// Returns the number of DomainKey-Signature header fields found in mimeData.
	int NumDomainKeySignatures(const CkByteData &mimeData);

	// Useful if your application is going to verify many emails from a single domain
	// (or a few domains). This eliminates the need to do a DNS lookup to fetch the
	// public key each time an email's DKIM or DomainKey signature is verified.
	// 
	// This method may be called multiple times -- once for each selector/domain to be
	// pre-fetched. The verify methods (VerifyDkimSignature and
	// VerifyDomainKeySignature) will use a pre-fetched key if the selector and domain
	// match.
	// 
	bool PrefetchPublicKey(const char *selector, const char *domain);

	// Unlocks the component allowing for the full functionality to be used. If this
	// method unexpectedly returns false, examine the contents of the LastErrorText
	// property to determine the reason for failure.
	bool UnlockComponent(const char *unlockCode);

	// Verifies the Nth DKIM-Signature header in the  mimeData. (In most cases, there is
	// only one signature.) The 1st signature is at sigIndex 0.
	// 
	// Important: Many anti-virus programs, such as AVG, will modify the MIME of an
	// email as it is received. This will cause DKIM signature verification to fail
	// because the body of the MIME is modified.
	// 
	bool VerifyDkimSignature(int sigIdx, const CkByteData &mimeData);

	// Verifies the Nth DomainKey-Signature header in the  mimeData. (In most cases, there
	// is only one signature.) The 1st signature is at sigIndex 0.
	// 
	// Important: Many anti-virus programs, such as AVG, will modify the MIME of an
	// email as it is received. This will cause DomainKey signature verification to
	// fail because the body of the MIME is modified.
	// 
	bool VerifyDomainKeySignature(int sigIdx, const CkByteData &mimeData);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
