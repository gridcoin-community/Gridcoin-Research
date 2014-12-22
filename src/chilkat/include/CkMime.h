// CkMime.h: interface for the CkMime class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkMime_H
#define _CkMime_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkCert;
class CkPrivateKey;
class CkByteData;
class CkStringArray;
class CkCsp;
class CkCertChain;
class CkXmlCertVault;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkMime
class CK_VISIBLE_PUBLIC CkMime  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkMime(const CkMime &);
	CkMime &operator=(const CkMime &);

    public:
	CkMime(void);
	virtual ~CkMime(void);

	static CkMime *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The boundary string for a multipart MIME message.
	// 
	// It is the value of the boundary attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/mixed; boundary="------------080707010302060306060800"
	// then the value of the Boundary property is
	// "------------080707010302060306060800".
	// 
	// When building multipart MIME messages, the boundary is automatically generated
	// by methods such as NewMultipartMixed, to be a unique and random string, so
	// explicitly setting the boundary is usually not necessary.
	// 
	void get_Boundary(CkString &str);
	// The boundary string for a multipart MIME message.
	// 
	// It is the value of the boundary attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/mixed; boundary="------------080707010302060306060800"
	// then the value of the Boundary property is
	// "------------080707010302060306060800".
	// 
	// When building multipart MIME messages, the boundary is automatically generated
	// by methods such as NewMultipartMixed, to be a unique and random string, so
	// explicitly setting the boundary is usually not necessary.
	// 
	const char *boundary(void);
	// The boundary string for a multipart MIME message.
	// 
	// It is the value of the boundary attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/mixed; boundary="------------080707010302060306060800"
	// then the value of the Boundary property is
	// "------------080707010302060306060800".
	// 
	// When building multipart MIME messages, the boundary is automatically generated
	// by methods such as NewMultipartMixed, to be a unique and random string, so
	// explicitly setting the boundary is usually not necessary.
	// 
	void put_Boundary(const char *newVal);

	// The value of the "charset" attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: text/plain; charset="iso-8859-1"
	// then the value of the Charset property is "iso-8859-1".
	void get_Charset(CkString &str);
	// The value of the "charset" attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: text/plain; charset="iso-8859-1"
	// then the value of the Charset property is "iso-8859-1".
	const char *charset(void);
	// The value of the "charset" attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: text/plain; charset="iso-8859-1"
	// then the value of the Charset property is "iso-8859-1".
	void put_Charset(const char *newVal);

	// The MIME content type, such as "text/plain", "text/html", "image/gif",
	// "multipart/alternative", "multipart/mixed", etc.
	// 
	// It is the value of the Content-Type header field, excluding any attributes. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/mixed; boundary="------------080707010302060306060800"
	// then the value of the ContentType property is "multipart/mixed".
	// 
	void get_ContentType(CkString &str);
	// The MIME content type, such as "text/plain", "text/html", "image/gif",
	// "multipart/alternative", "multipart/mixed", etc.
	// 
	// It is the value of the Content-Type header field, excluding any attributes. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/mixed; boundary="------------080707010302060306060800"
	// then the value of the ContentType property is "multipart/mixed".
	// 
	const char *contentType(void);
	// The MIME content type, such as "text/plain", "text/html", "image/gif",
	// "multipart/alternative", "multipart/mixed", etc.
	// 
	// It is the value of the Content-Type header field, excluding any attributes. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/mixed; boundary="------------080707010302060306060800"
	// then the value of the ContentType property is "multipart/mixed".
	// 
	void put_ContentType(const char *newVal);

	// Returns the current date/time in RFC 822 format.
	void get_CurrentDateTime(CkString &str);
	// Returns the current date/time in RFC 822 format.
	const char *currentDateTime(void);

	// The value of the Content-Disposition header field, excluding any attributes. For
	// example, if the Content-Disposition header is this:
	// Content-Disposition: attachment; filename="starfish.gif"
	// then the value of the Disposition property is "attachment".
	void get_Disposition(CkString &str);
	// The value of the Content-Disposition header field, excluding any attributes. For
	// example, if the Content-Disposition header is this:
	// Content-Disposition: attachment; filename="starfish.gif"
	// then the value of the Disposition property is "attachment".
	const char *disposition(void);
	// The value of the Content-Disposition header field, excluding any attributes. For
	// example, if the Content-Disposition header is this:
	// Content-Disposition: attachment; filename="starfish.gif"
	// then the value of the Disposition property is "attachment".
	void put_Disposition(const char *newVal);

	// The value of the Content-Transfer-Encoding header field. Typical values are
	// "base64", "quoted-printable", "7bit", "8bit", "binary", etc. For example, if the
	// Content-Transfer-Encoding header is this:
	// Content-Transfer-Encoding: base64
	// then the value of the Encoding property is "base64".
	void get_Encoding(CkString &str);
	// The value of the Content-Transfer-Encoding header field. Typical values are
	// "base64", "quoted-printable", "7bit", "8bit", "binary", etc. For example, if the
	// Content-Transfer-Encoding header is this:
	// Content-Transfer-Encoding: base64
	// then the value of the Encoding property is "base64".
	const char *encoding(void);
	// The value of the Content-Transfer-Encoding header field. Typical values are
	// "base64", "quoted-printable", "7bit", "8bit", "binary", etc. For example, if the
	// Content-Transfer-Encoding header is this:
	// Content-Transfer-Encoding: base64
	// then the value of the Encoding property is "base64".
	void put_Encoding(const char *newVal);

	// The value of the "filename" attribute of the Content-Disposition header field.
	// For example, if the Content-Disposition header is this:
	// Content-Disposition: attachment; filename="starfish.gif"
	// then the value of the Filename property is "starfish.gif".
	void get_Filename(CkString &str);
	// The value of the "filename" attribute of the Content-Disposition header field.
	// For example, if the Content-Disposition header is this:
	// Content-Disposition: attachment; filename="starfish.gif"
	// then the value of the Filename property is "starfish.gif".
	const char *filename(void);
	// The value of the "filename" attribute of the Content-Disposition header field.
	// For example, if the Content-Disposition header is this:
	// Content-Disposition: attachment; filename="starfish.gif"
	// then the value of the Filename property is "starfish.gif".
	void put_Filename(const char *newVal);

	// The value of the "micalg" attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/signed; protocol="application/x-pkcs7-signature"; micalg=sha1; 
	//   boundary="------------ms000908010507020408060303"
	// then the value of the Micalg property is "sha".
	// 
	// Note: The micalg attribute is only present in PKCS7 signed MIME. Setting the
	// Micalg property has the effect of choosing the hash algorithm used w/ signing.
	// Possible choices are "sha1", "md5", "sha256", "sha384", and "sha512". However,
	// it is preferable to set the signing hash algorithm by setting the SigningHashAlg
	// property instead.
	// 
	void get_Micalg(CkString &str);
	// The value of the "micalg" attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/signed; protocol="application/x-pkcs7-signature"; micalg=sha1; 
	//   boundary="------------ms000908010507020408060303"
	// then the value of the Micalg property is "sha".
	// 
	// Note: The micalg attribute is only present in PKCS7 signed MIME. Setting the
	// Micalg property has the effect of choosing the hash algorithm used w/ signing.
	// Possible choices are "sha1", "md5", "sha256", "sha384", and "sha512". However,
	// it is preferable to set the signing hash algorithm by setting the SigningHashAlg
	// property instead.
	// 
	const char *micalg(void);
	// The value of the "micalg" attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/signed; protocol="application/x-pkcs7-signature"; micalg=sha1; 
	//   boundary="------------ms000908010507020408060303"
	// then the value of the Micalg property is "sha".
	// 
	// Note: The micalg attribute is only present in PKCS7 signed MIME. Setting the
	// Micalg property has the effect of choosing the hash algorithm used w/ signing.
	// Possible choices are "sha1", "md5", "sha256", "sha384", and "sha512". However,
	// it is preferable to set the signing hash algorithm by setting the SigningHashAlg
	// property instead.
	// 
	void put_Micalg(const char *newVal);

	// The value of the "name" attribute of the Content-Type header field. For example,
	// if the Content-Type header is this:
	// Content-Type: image/gif; name="starfish.gif"
	// then the value of the Name property is "starfish.gif".
	void get_Name(CkString &str);
	// The value of the "name" attribute of the Content-Type header field. For example,
	// if the Content-Type header is this:
	// Content-Type: image/gif; name="starfish.gif"
	// then the value of the Name property is "starfish.gif".
	const char *name(void);
	// The value of the "name" attribute of the Content-Type header field. For example,
	// if the Content-Type header is this:
	// Content-Type: image/gif; name="starfish.gif"
	// then the value of the Name property is "starfish.gif".
	void put_Name(const char *newVal);

	// The number of certificates found when decrypting S/MIME. This property is set
	// after UnwrapSecurity is called.
	int get_NumEncryptCerts(void);

	// The number of header fields. Header field names and values can be retrieved by
	// index (starting at 0) by calling GetHeaderFieldName and GetHeaderFieldValue.
	int get_NumHeaderFields(void);

	// MIME messages are composed of parts in a tree structure. The NumParts property
	// contains the number of direct children. To traverse an entire MIME tree, one
	// would recursively descend the tree structure by iterating from 0 to NumParts-1,
	// calling GetPart to get each direct child MIME object. The traversal would
	// continue by iterating over each child's parts, and so on.
	int get_NumParts(void);

	// The number of certificates found when verifying signature(s). This property is
	// set after UnwrapSecurity is called.
	int get_NumSignerCerts(void);

	// When the MIME is encrypted (using PKCS7 public-key encryption), this selects the
	// underlying symmetric encryption algorithm. Possible values are: "aes", "des",
	// "3des", and "rc2".
	void get_Pkcs7CryptAlg(CkString &str);
	// When the MIME is encrypted (using PKCS7 public-key encryption), this selects the
	// underlying symmetric encryption algorithm. Possible values are: "aes", "des",
	// "3des", and "rc2".
	const char *pkcs7CryptAlg(void);
	// When the MIME is encrypted (using PKCS7 public-key encryption), this selects the
	// underlying symmetric encryption algorithm. Possible values are: "aes", "des",
	// "3des", and "rc2".
	void put_Pkcs7CryptAlg(const char *newVal);

	// When the MIME is encrypted (using PKCS7 public-key encryption), this selects the
	// key length of the underlying symmetric encryption algorithm. The possible values
	// allowed depend on the Pkcs7CryptAlg property. For "aes", the key length may be
	// 128, 192, or 256. For "3des" the key length must be 192. For "des" the key
	// length must be 40. For "rc2" the key length can be 40, 56, 64, or 128.
	int get_Pkcs7KeyLength(void);
	// When the MIME is encrypted (using PKCS7 public-key encryption), this selects the
	// key length of the underlying symmetric encryption algorithm. The possible values
	// allowed depend on the Pkcs7CryptAlg property. For "aes", the key length may be
	// 128, 192, or 256. For "3des" the key length must be 192. For "des" the key
	// length must be 40. For "rc2" the key length can be 40, 56, 64, or 128.
	void put_Pkcs7KeyLength(int newVal);

	// The value of the "protocol" attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/signed; protocol="application/x-pkcs7-signature"; micalg=sha1; 
	//   boundary="------------ms000908010507020408060303"
	// then the value of the Protocol property is "application/x-pkcs7-signature".
	void get_Protocol(CkString &str);
	// The value of the "protocol" attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/signed; protocol="application/x-pkcs7-signature"; micalg=sha1; 
	//   boundary="------------ms000908010507020408060303"
	// then the value of the Protocol property is "application/x-pkcs7-signature".
	const char *protocol(void);
	// The value of the "protocol" attribute of the Content-Type header field. For
	// example, if the Content-Type header is this:
	// Content-Type: multipart/signed; protocol="application/x-pkcs7-signature"; micalg=sha1; 
	//   boundary="------------ms000908010507020408060303"
	// then the value of the Protocol property is "application/x-pkcs7-signature".
	void put_Protocol(const char *newVal);

	// Selects the underlying hash algorithm used when creating signed (PKCS7) MIME.
	// Possible values are "sha1", "sha256", "sha384", "sha512", "md5", and "md2".
	void get_SigningHashAlg(CkString &str);
	// Selects the underlying hash algorithm used when creating signed (PKCS7) MIME.
	// Possible values are "sha1", "sha256", "sha384", "sha512", "md5", and "md2".
	const char *signingHashAlg(void);
	// Selects the underlying hash algorithm used when creating signed (PKCS7) MIME.
	// Possible values are "sha1", "sha256", "sha384", "sha512", "md5", and "md2".
	void put_SigningHashAlg(const char *newVal);

	// Controls whether extra (informative) header fields are added to the MIME message
	// when unwrapping security.
	bool get_UnwrapExtras(void);
	// Controls whether extra (informative) header fields are added to the MIME message
	// when unwrapping security.
	void put_UnwrapExtras(bool newVal);

	// Controls whether the boilerplate text "This is a multi-part message in MIME
	// format." is used as the body content of a multipart MIME part.
	bool get_UseMmDescription(void);
	// Controls whether the boilerplate text "This is a multi-part message in MIME
	// format." is used as the body content of a multipart MIME part.
	void put_UseMmDescription(bool newVal);

	// If true, then the Content-Type header fields created by Chilkat will use
	// "x-pkcs7" instead of simply "pkcs7" . For example:
	// Content-Type: multipart/signed;
	// 	boundary="----=_NextPart_af8_0422_dbec3a60.7178e470";
	// 	protocol="application/x-pkcs7-signature"; micalg=sha1
	// 
	// or
	// 
	// Content-Type: application/x-pkcs7-mime; name="smime.p7m"
	// If false, then the "pcks7" is used. For example:
	// Content-Type: multipart/signed;
	// 	boundary="----=_NextPart_af8_0422_dbec3a60.7178e470";
	// 	protocol="application/pkcs7-signature"; micalg=sha1
	// 
	// or
	// 
	// Content-Type: application/pkcs7-mime; name="smime.p7m"
	// The default value of this property is true, meaning that "x-" is used by
	// default.
	bool get_UseXPkcs7(void);
	// If true, then the Content-Type header fields created by Chilkat will use
	// "x-pkcs7" instead of simply "pkcs7" . For example:
	// Content-Type: multipart/signed;
	// 	boundary="----=_NextPart_af8_0422_dbec3a60.7178e470";
	// 	protocol="application/x-pkcs7-signature"; micalg=sha1
	// 
	// or
	// 
	// Content-Type: application/x-pkcs7-mime; name="smime.p7m"
	// If false, then the "pcks7" is used. For example:
	// Content-Type: multipart/signed;
	// 	boundary="----=_NextPart_af8_0422_dbec3a60.7178e470";
	// 	protocol="application/pkcs7-signature"; micalg=sha1
	// 
	// or
	// 
	// Content-Type: application/pkcs7-mime; name="smime.p7m"
	// The default value of this property is true, meaning that "x-" is used by
	// default.
	void put_UseXPkcs7(bool newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Computes the size of the MIME body and adds a Content-Length header field with
	// the computed value. If the MIME body is non-multipart, the Content-Length is
	// just the size of the content. If the MIME is multipart, then the Content-Length
	// is the sum of all the sub-parts. Calling this method more than once causes the
	// Content-Length header to be re-computed and updated.
	void AddContentLength(void);

	// Signs the message using the certificate provided. If successful, the message is
	// converted to "multipart/signed" and the original message will be contained in
	// the first sub-part.
	bool AddDetachedSignature(const CkCert &cert);

	// Same as AddDetachedSignature, except an extra argument is provided to control
	// whether header fields from the calling MIME object are transferred to the
	// content part of the multipart/signed object. This method transforms the calling
	// object into a multipart/signed MIME with two sub-parts. The first contains the
	// original content of the calling object, and the second contains the digital
	// signature.
	bool AddDetachedSignature2(const CkCert &cert, bool transferHeaderFields);

	// Adds a detached signature using a certificate and it's associated private key.
	// This method would be used when the private key is external to the certificate --
	// for example, if a PFX/P12 file is not used, but instead a pair of .cer and .pem
	// files are used (one for the certificate and one for the associated private key).
	bool AddDetachedSignaturePk(const CkCert &cert, const CkPrivateKey &privateKey);

	// Same as AddDetachedSignaturePk, except an extra argument is provided to control
	// whether header fields from the calling MIME object are transferred to the
	// content part of the multipart/signed object. This method transforms the calling
	// object into a multipart/signed MIME with two sub-parts. The first contains the
	// original content of the calling object, and the second contains the digital
	// signature.
	bool AddDetachedSignaturePk2(const CkCert &cert, const CkPrivateKey &privateKey, bool transferHeaderFields);

	// Adds a certificate to the object's internal list of certificates to be used when
	// the EncryptN method is called. (See the EncryptN method for more information.)
	// The internal list may be cleared by calling ClearEncryptCerts.
	bool AddEncryptCert(CkCert &cert);

	// Adds a header field to the MIME.
	bool AddHeaderField(const char *name, const char *value);

	// Adds a PFX to the object's internal list of sources to be searched for
	// certificates and private keys when decrypting . Multiple PFX sources can be
	// added by calling this method once for each. (On the Windows operating system,
	// the registry-based certificate stores are also automatically searched, so it is
	// commonly not required to explicitly add PFX sources.)
	// 
	// The pfxFileData contains the bytes of a PFX file (also known as PKCS12 or .p12).
	// 
	bool AddPfxSourceData(const CkByteData &pfxData, const char *password);

	// Adds a PFX file to the object's internal list of sources to be searched for
	// certificates and private keys when decrypting. Multiple PFX files can be added
	// by calling this method once for each. (On the Windows operating system, the
	// registry-based certificate stores are also automatically searched, so it is
	// commonly not required to explicitly add PFX sources.)
	// 
	// The ARG1 contains the bytes of a PFX file (also known as PKCS12 or .p12).
	// 
	bool AddPfxSourceFile(const char *pfxFilePath, const char *password);

	// Appends a MIME message to the sub-parts of this message. Arbitrarily complex
	// messages with unlimited nesting levels can be created. If the calling Mime
	// object is not already multipart, it is automatically converted to
	// multipart/mixed first.
	bool AppendPart(const CkMime &mime);

	// Loads a file and creates a Mime message object using the file extension to
	// determine the content type, and adds it as a sub-part to the calling object.
	bool AppendPartFromFile(const char *filename);

	// When the body of a MIME part contains PKCS7 (ASN.1 in DER format,
	// base64-encoded), this method can be used to convert the ASN.1 to an XML format
	// for inspection. Here is an example of how an ASN.1 body might look:
	// Content-Type: application/x-pkcs7-mime;
	// 	name="smime.p7m"; smime-type="signed-data"
	// Content-Transfer-Encoding: base64
	// Content-Disposition: attachment; filename="smime.p7m"
	// 
	// MIIXXAYJKoZIhvcNAQcCoIIXTTCCF0kCAQExCzAJBgUrDgMCGgUAMFoGCSqGSIb3DQEHAaBNBEtD
	// b250ZW50LVR5cGU6IHRleHQvcGxhaW4NCkNvbnRlbnQtVHJhbnNmZXItRW5jb2Rpbmc6IDdiaXQN
	// Cg0KdGhpcyBpcyBhIHRlc3SgghI/MIIE3jCCA8agAwIBAgICAwEwDQYJKoZIhvcNAQEFBQAwYzEL
	// ...
	// The XML produced would look something like this:
	// _LT_?xml version="1.0" encoding="utf-8" ?>
	// _LT_sequence>
	//     _LT_oid>1.2.840.113549.1.7.2_LT_/oid>
	//     _LT_contextSpecific tag="0" constructed="1">
	//         _LT_sequence>
	//             _LT_int>01_LT_/int>
	//             _LT_set>
	//                 _LT_sequence>
	//                     _LT_oid>1.3.14.3.2.26_LT_/oid>
	//                     _LT_null />
	//                 _LT_/sequence>
	//             _LT_/set>
	//             _LT_sequence>
	//                 _LT_oid>1.2.840.113549.1.7.1_LT_/oid>
	//                 _LT_contextSpecific tag="0" constructed="1">
	// ...
	bool AsnBodyToXml(CkString &outStr);
	// When the body of a MIME part contains PKCS7 (ASN.1 in DER format,
	// base64-encoded), this method can be used to convert the ASN.1 to an XML format
	// for inspection. Here is an example of how an ASN.1 body might look:
	// Content-Type: application/x-pkcs7-mime;
	// 	name="smime.p7m"; smime-type="signed-data"
	// Content-Transfer-Encoding: base64
	// Content-Disposition: attachment; filename="smime.p7m"
	// 
	// MIIXXAYJKoZIhvcNAQcCoIIXTTCCF0kCAQExCzAJBgUrDgMCGgUAMFoGCSqGSIb3DQEHAaBNBEtD
	// b250ZW50LVR5cGU6IHRleHQvcGxhaW4NCkNvbnRlbnQtVHJhbnNmZXItRW5jb2Rpbmc6IDdiaXQN
	// Cg0KdGhpcyBpcyBhIHRlc3SgghI/MIIE3jCCA8agAwIBAgICAwEwDQYJKoZIhvcNAQEFBQAwYzEL
	// ...
	// The XML produced would look something like this:
	// _LT_?xml version="1.0" encoding="utf-8" ?>
	// _LT_sequence>
	//     _LT_oid>1.2.840.113549.1.7.2_LT_/oid>
	//     _LT_contextSpecific tag="0" constructed="1">
	//         _LT_sequence>
	//             _LT_int>01_LT_/int>
	//             _LT_set>
	//                 _LT_sequence>
	//                     _LT_oid>1.3.14.3.2.26_LT_/oid>
	//                     _LT_null />
	//                 _LT_/sequence>
	//             _LT_/set>
	//             _LT_sequence>
	//                 _LT_oid>1.2.840.113549.1.7.1_LT_/oid>
	//                 _LT_contextSpecific tag="0" constructed="1">
	// ...
	const char *asnBodyToXml(void);

	// Clears the internal list of certificates added by previous calls to the
	// AddEncryptCert method. (See the EncryptN method for information about encrypting
	// using multiple certificates.)
	void ClearEncryptCerts(void);

	// Returns true if the MIME message contains encrypted parts.
	// 
	// Note: This method examines the MIME as-is. If UnwrapSecurity is called and it is
	// successful, then the MIME should no longer contain encrypted parts, and this
	// method would return 0.
	// 
	// Note: If a signed MIME message is then encrypted, then it is not possible to
	// know that the MIME is both encrypted and signed until UnwrapSecurity is called.
	// (In other words, it is not possible to know the contents of the encrypted MIME
	// until it is decrypted.) Therefore, the ContainsSignedParts method would return
	// false.
	// 
	bool ContainsEncryptedParts(void);

	// Returns true if the MIME message contains signed parts.
	// 
	// Note: This method examines the MIME as-is. If UnwrapSecurity is called and it is
	// successful, then the MIME should no longer contain signed parts, and this method
	// would return 0.
	// 
	// Note: If a signed MIME message is then encrypted, then it is not possible to
	// know that the MIME is both encrypted and signed until UnwrapSecurity is called.
	// (In other words, it is not possible to know the contents of the encrypted MIME
	// until it is decrypted.) Therefore, the ContainsSignedParts method would return
	// false.
	// 
	// Note: The same concept also applies to opaque signatures, such as with the MIME
	// produced by calling ConvertToSigned.
	// 
	bool ContainsSignedParts(void);

	// Changes the content-transfer-encoding to "base64" for all 8bit or binary MIME
	// subparts. This allows for the MIME to be exported as a string via the GetMime
	// method.
	void Convert8Bit(void);

	// Converts existing MIME to a multipart/alternative. This is accomplished by
	// creating a new outermost multipart/alternative MIME part. The existing MIME is
	// moved into the 1st (and only) sub-part of the new multipart/alternative
	// enclosure. Header fields from the original top-level MIME part are transferred
	// to the new top-level multipart/alternative header, except for Content-Type,
	// Content-Transfer-Encoding, and Content-Disposition. For example, the following
	// simple plain-text MIME is converted as follows:
	// 
	// Original:
	// MIME-Version: 1.0
	// Date: Sun, 11 Aug 2013 11:18:44 -0500
	// Message-ID: Content-Type: text/plain
	// Content-Transfer-Encoding: quoted-printable
	// X-Priority: 3 (Normal)
	// Subject: this is the subject.
	// From: "Chilkat Software" 
	// To: "Chilkat Sales" This is the plain-text body.
	// 
	// After Converting:
	// MIME-Version: 1.0
	// Date: Sun, 11 Aug 2013 11:18:44 -0500
	// Message-ID: X-Priority: 3 (Normal)
	// Subject: this is the subject.
	// From: "Chilkat Software" 
	// To: "Chilkat Sales" Content-Type: multipart/alternative;
	// 	boundary="------------040101040804050401050400_.ALT"
	// 
	// --------------040101040804050401050400_.ALT
	// Content-Type: text/plain
	// Content-Transfer-Encoding: quoted-printable
	// 
	// This is the plain-text body.
	// --------------040101040804050401050400_.ALT--
	// 
	bool ConvertToMultipartAlt(void);

	// Converts existing MIME to a multipart/mixed. This is accomplished by creating a
	// new outermost multipart/mixed MIME part. The existing MIME is moved into the 1st
	// (and only) sub-part of the new multipart/mixed enclosure. Header fields from the
	// original top-level MIME part are transferred to the new top-level
	// multipart/mixed header, except for Content-Type, Content-Transfer-Encoding, and
	// Content-Disposition. For example, the following simple plain-text MIME is
	// converted as follows:
	// 
	// Original:
	// MIME-Version: 1.0
	// Date: Sun, 11 Aug 2013 11:27:04 -0500
	// Message-ID: Content-Type: text/plain
	// Content-Transfer-Encoding: quoted-printable
	// X-Priority: 3 (Normal)
	// Subject: this is the subject.
	// From: "Chilkat Software" 
	// To: "Chilkat Sales" This is the plain-text body.
	// 
	// After Converting:
	// MIME-Version: 1.0
	// Date: Sun, 11 Aug 2013 11:27:04 -0500
	// Message-ID: X-Priority: 3 (Normal)
	// Subject: this is the subject.
	// From: "Chilkat Software" 
	// To: "Chilkat Sales" Content-Type: multipart/mixed;
	// 	boundary="------------050508060709030908040207"
	// 
	// --------------050508060709030908040207
	// Content-Type: text/plain
	// Content-Transfer-Encoding: quoted-printable
	// 
	// This is the plain-text body.
	// --------------050508060709030908040207--
	// 
	bool ConvertToMultipartMixed(void);

	// Digitally signs a MIME message. The MIME is converted to an
	// application/x-pkcs7-mime which is a PKCS7 signature that includes both the
	// original MIME message and the signature. This is different than
	// AddDetachedSignature, where the signature is appended to the MIME.
	// 
	// Note: This is commonly referred to as an "opaque" signature.
	// 
	bool ConvertToSigned(const CkCert &cert);

	// Digitally signs the MIME to convert it to an "opaque" signed message using a
	// certificate and it's associated private key. This method would be used when the
	// private key is external to the certificate -- for example, if a PFX/P12 file is
	// not used, but instead a pair of .cer and .pem files are used (one for the
	// certificate and one for the associated private key).
	bool ConvertToSignedPk(const CkCert &cert, const CkPrivateKey &privateKey);

	// Decrypts PKCS7 encrypted MIME (also known as S/MIME). Information about the
	// certificates required for decryption is always embedded within PKCS7 encrypted
	// MIME. This method will automatically find and use the certificate + private key
	// required from three possible sources:
	//     PFX files that were provided in one or more calls to AddPfxSourceData or
	//     AddPfxSourceFile.
	//     Certificates found in an XML certificate vault provided by calling the
	//     UseCertVault method.
	//     (On Windows systems) Certificates found in the system's registry-based
	//     certificate stores.
	bool Decrypt(void);

	// The same as Decrypt, but useful when the certificate and private key are
	// available in separate files (as opposed to a single file such as a .pfx/.p12).
	bool Decrypt2(const CkCert &cert, const CkPrivateKey &privateKey);

	// Decrypts MIME using a specific PFX ( also known as PKCS12, which is a file
	// format commonly used to store private keys with accompanying public key
	// certificates, protected with a password-based symmetric key). This method allows
	// the bytes of the PKCS12 file to be passed directly, thus allowing PKCS12's to be
	// persisted and retrieved from non-file-based locations, such as in LDAP or a
	// database.
	bool DecryptUsingPfxData(const CkByteData &pfxData, const char *password);

	// Decrypts MIME using a specific PFX file (also known as PKCS12) as the source for
	// any required certificates and private keys. (Note: .pfx and .p12 files are both
	// PKCS12 format.)
	bool DecryptUsingPfxFile(const char *pfxFilePath, const char *password);

	// Encrypts the MIME to create PKCS7 encrypted MIME. A digital certificate (which
	// always contains a public-key) is used to encrypt.
	bool Encrypt(const CkCert &cert);

	// Encrypt MIME using any number of digital certificates. Each certificate to be
	// used must first be added by calling AddEncryptCert (once per certificate). See
	// the example code below:
	bool EncryptN(void);

#ifndef MOBILE_MIME
	// Recursively descends through the parts of a MIME message and extracts all parts
	// having a filename to a file. The files are created in dirPath. Returns a
	// (Ck)StringArray object containing the names of the files created. The filenames
	// are obtained from the "filename" attribute of the content-disposition header. If
	// a filename does not exist, then the MIME part is not saved to a file.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArray *ExtractPartsToFiles(const char *dirPath);
#endif

	// Finds and returns the issuer certificate. If the certificate is a root or
	// self-issued, then the certificate returned is a copy of the caller certificate.
	// The caller is responsible for deleting the object returned by this method.
	CkCert *FindIssuer(CkCert &cert);

	// Returns the body of the MIME message as a block of binary data. The body is
	// automatically converted from its encoding type, such as base64 or
	// quoted-printable, before being returned.
	bool GetBodyBinary(CkByteData &outData);

	// Returns the body of the MIME message as a string. The body is automatically
	// converted from its encoding type, such as base64 or quoted-printable, before
	// being returned.
	bool GetBodyDecoded(CkString &outStr);
	// Returns the body of the MIME message as a string. The body is automatically
	// converted from its encoding type, such as base64 or quoted-printable, before
	// being returned.
	const char *getBodyDecoded(void);
	// Returns the body of the MIME message as a string. The body is automatically
	// converted from its encoding type, such as base64 or quoted-printable, before
	// being returned.
	const char *bodyDecoded(void);

	// Returns the body of the MIME message as a String. The body is explicitly not
	// decoded from it's encoding type, so if it was represented in Base64, you will
	// get the Base64 encoded body, as an example.
	bool GetBodyEncoded(CkString &outStr);
	// Returns the body of the MIME message as a String. The body is explicitly not
	// decoded from it's encoding type, so if it was represented in Base64, you will
	// get the Base64 encoded body, as an example.
	const char *getBodyEncoded(void);
	// Returns the body of the MIME message as a String. The body is explicitly not
	// decoded from it's encoding type, so if it was represented in Base64, you will
	// get the Base64 encoded body, as an example.
	const char *bodyEncoded(void);

	// Returns the Nth certificate found when decrypting. The EncryptCerts property
	// contains the number of certificates.
	// The caller is responsible for deleting the object returned by this method.
	CkCert *GetEncryptCert(int index);

	// Returns the entire MIME body, including all sub-parts.
	bool GetEntireBody(CkString &outStr);
	// Returns the entire MIME body, including all sub-parts.
	const char *getEntireBody(void);
	// Returns the entire MIME body, including all sub-parts.
	const char *entireBody(void);

	// Returns the MIME header.
	bool GetEntireHead(CkString &outStr);
	// Returns the MIME header.
	const char *getEntireHead(void);
	// Returns the MIME header.
	const char *entireHead(void);

	// Returns the value of a MIME header field. fieldName is case-insensitive.
	bool GetHeaderField(const char *name, CkString &outStr);
	// Returns the value of a MIME header field. fieldName is case-insensitive.
	const char *getHeaderField(const char *name);
	// Returns the value of a MIME header field. fieldName is case-insensitive.
	const char *headerField(const char *name);

	// Parses a MIME header field and returns the value of an attribute. MIME header
	// fields w/ attributes are formatted like this:
	// Header-Name:  value;  attrName1="value1"; attrName2="value2"; ....  attrNameN="valueN"
	// Semi-colons separate attribute name=value pairs. The Content-Type header field
	// often contains attributes. Here is an example:
	// Content-Type: multipart/signed;
	// 	protocol="application/x-pkcs7-signature";
	// 	micalg=SHA1;
	// 	boundary="----=_NextPart_000_0000_01CB03E4.D0BAF010"
	// In the above example, to access the value of the "protocol" attribute, call
	// GetHeaderFieldAttribute("Content-Type", "protocol");
	bool GetHeaderFieldAttribute(const char *name, const char *attrName, CkString &outStr);
	// Parses a MIME header field and returns the value of an attribute. MIME header
	// fields w/ attributes are formatted like this:
	// Header-Name:  value;  attrName1="value1"; attrName2="value2"; ....  attrNameN="valueN"
	// Semi-colons separate attribute name=value pairs. The Content-Type header field
	// often contains attributes. Here is an example:
	// Content-Type: multipart/signed;
	// 	protocol="application/x-pkcs7-signature";
	// 	micalg=SHA1;
	// 	boundary="----=_NextPart_000_0000_01CB03E4.D0BAF010"
	// In the above example, to access the value of the "protocol" attribute, call
	// GetHeaderFieldAttribute("Content-Type", "protocol");
	const char *getHeaderFieldAttribute(const char *name, const char *attrName);
	// Parses a MIME header field and returns the value of an attribute. MIME header
	// fields w/ attributes are formatted like this:
	// Header-Name:  value;  attrName1="value1"; attrName2="value2"; ....  attrNameN="valueN"
	// Semi-colons separate attribute name=value pairs. The Content-Type header field
	// often contains attributes. Here is an example:
	// Content-Type: multipart/signed;
	// 	protocol="application/x-pkcs7-signature";
	// 	micalg=SHA1;
	// 	boundary="----=_NextPart_000_0000_01CB03E4.D0BAF010"
	// In the above example, to access the value of the "protocol" attribute, call
	// GetHeaderFieldAttribute("Content-Type", "protocol");
	const char *headerFieldAttribute(const char *name, const char *attrName);

	// Returns the Nth MIME header field name.
	bool GetHeaderFieldName(int index, CkString &outStr);
	// Returns the Nth MIME header field name.
	const char *getHeaderFieldName(int index);
	// Returns the Nth MIME header field name.
	const char *headerFieldName(int index);

	// Returns the Nth MIME header field value.
	bool GetHeaderFieldValue(int index, CkString &outStr);
	// Returns the Nth MIME header field value.
	const char *getHeaderFieldValue(int index);
	// Returns the Nth MIME header field value.
	const char *headerFieldValue(int index);

	// Returns a string containing the complete MIME message, including all sub-parts.
	bool GetMime(CkString &outStr);
	// Returns a string containing the complete MIME message, including all sub-parts.
	const char *getMime(void);
	// Returns a string containing the complete MIME message, including all sub-parts.
	const char *mime(void);

	// Returns a byte array containing the complete MIME message, including all
	// sub-parts.
	bool GetMimeBytes(CkByteData &outBytes);

	// Returns the Nth sub-part of the MIME message. Indexing begins at 0.
	// The caller is responsible for deleting the object returned by this method.
	CkMime *GetPart(int index);

	// Returns the signature signing date/time for the Nth signature. The number of
	// signatures (i.e. signer certs) is indicated by the NumSignerCerts property. The
	// HasSignatureSigningTime method may be called to determine if a signature
	// timestamp is available. The index of the 1st signature signing time is 0.
	bool GetSignatureSigningTime(int index, SYSTEMTIME &outSysTime);

	// The same as the GetSignatureSigningTime method, but returns tjhe date/time in
	// RFC822 string format.
	bool GetSignatureSigningTimeStr(int index, CkString &outStr);
	// The same as the GetSignatureSigningTime method, but returns tjhe date/time in
	// RFC822 string format.
	const char *getSignatureSigningTimeStr(int index);
	// The same as the GetSignatureSigningTime method, but returns tjhe date/time in
	// RFC822 string format.
	const char *signatureSigningTimeStr(int index);

	// Returns the Nth digital certificate used to sign the MIME message. Indexing
	// begins at 0.
	// The caller is responsible for deleting the object returned by this method.
	CkCert *GetSignerCert(int index);

	// Converts the MIME (or S/MIME) message to XML and returns the XML as a string.
	bool GetXml(CkString &outStr);
	// Converts the MIME (or S/MIME) message to XML and returns the XML as a string.
	const char *getXml(void);
	// Converts the MIME (or S/MIME) message to XML and returns the XML as a string.
	const char *xml(void);

	// Returns true if the Nth signature included a timestamp that recorded the
	// signing time. The number of signatures (i.e. signer certs) is indicated by the
	// NumSignerCerts property. (In most cases, the number of signer certs is 1.) The
	// signing time can be obtained via the GetSignatureSigningTime or
	// GetSignatureSigningTimeStr methods. The index of the 1st signature signing time
	// is 0.
	bool HasSignatureSigningTime(int index);

	// Return true if the MIME message contains application data, otherwise returns
	// false.
	bool IsApplicationData(void);

	// Return true if this MIME message is an attachment, otherwise returns false.
	// A MIME message is considered an attachment if the Content-Disposition header
	// field contains the value "attachment".
	bool IsAttachment(void);

	// Return true if the MIME message contains audio data, otherwise returns
	// false.
	bool IsAudio(void);

	// Returns true if the MIME message is PKCS7 encrypted, otherwise returns
	// false.
	bool IsEncrypted(void);

	// Return true if the MIME body is HTML, otherwise returns false.
	bool IsHtml(void);

	// Return true if the MIME message contains image data, otherwise returns
	// false.
	bool IsImage(void);

	// Return true if the MIME message is multipart (multipart/mixed,
	// multipart/related, multipart/alternative, etc.), otherwise returns false.
	bool IsMultipart(void);

	// Return true if the MIME message is multipart/alternative, otherwise returns
	// false.
	bool IsMultipartAlternative(void);

	// Return true if the MIME message is multipart/mixed, otherwise returns false.
	bool IsMultipartMixed(void);

	// Return true if the MIME message is multipart/related, otherwise returns
	// false.
	bool IsMultipartRelated(void);

	// Return true if the MIME message body is plain text, otherwise returns false.
	bool IsPlainText(void);

	// Return true if the MIME message is PKCS7 digitally signed, otherwise returns
	// false.
	bool IsSigned(void);

	// Return true if the MIME message body is any text content type, such as
	// text/plain, text/html, text/xml, etc., otherwise returns false.
	bool IsText(void);

	// Returns true if the component is already unlocked, otherwise returns false.
	bool IsUnlocked(void);

	// Return true if the MIME message contains video data, otherwise returns
	// false.
	bool IsVideo(void);

	// Return true if the MIME message body is XML, otherwise returns false.
	bool IsXml(void);

	// Discards the current contents of the MIME object and loads a new MIME message
	// from a string.
	bool LoadMime(const char *mimeMsg);

	// Loads a MIME document from an in-memory byte array.
	bool LoadMimeBytes(const CkByteData &binData);

	// Discards the current contents of the MIME object and loads a new MIME message
	// from a file.
	bool LoadMimeFile(const char *fileName);

	// Converts XML to MIME and replaces the MIME object's contents with the converted
	// XML.
	bool LoadXml(const char *xml);

	// Converts XML to MIME and replaces the MIME object's contents with the converted
	// XML.
	bool LoadXmlFile(const char *fileName);

	// Clears the Mime object and initializes it such that the header contains a
	// "content-type: message/rfc822" line and the body is the MIME text of the Mime
	// object passed to the method.
	bool NewMessageRfc822(const CkMime &mimeObject);

	// Discards the current MIME message header fields and contents, if any, an
	// initializes the MIME object to be an empty mulipart/alternative message.
	bool NewMultipartAlternative(void);

	// Discards the current MIME message header fields and contents, if any, an
	// initializes the MIME object to be an empty mulipart/mixed message.
	bool NewMultipartMixed(void);

	// Discards the current MIME message header fields and contents, if any, an
	// initializes the MIME object to be an empty mulipart/related message.
	bool NewMultipartRelated(void);

	// Removes a header field from the MIME header. If  bAllOccurances is true, then all
	// occurances of the header field are removed. Otherwise, only the 1st occurance is
	// removed.
	void RemoveHeaderField(const char *name, bool bAllOccurances);

	// Removes the Nth subpart from the MIME message.
	bool RemovePart(int index);

	// Saves the MIME message body to a file. If the body is base64 or quoted-printable
	// encoded, it is automatically decoded.
	bool SaveBody(const char *filename);

	// Saves the MIME message to a file, in MIME format. (This is the same as the .EML
	// format used by Microsoft Outlook Express.)
	bool SaveMime(const char *filename);

	// Converts the MIME message to XML and saves to an XML file.
	bool SaveXml(const char *filename);

	// Sets the MIME body content to a text string.
	void SetBody(const char *str);

	// Sets the MIME message body from a byte array.
	bool SetBodyFromBinary(const CkByteData &binData);

	// Sets the MIME message body from a Base64 or Quoted-Printable encoded string.
	bool SetBodyFromEncoded(const char *encoding, const char *str);

	// Sets the MIME message body from the contents of a file. Note: A MIME message
	// consists of a header and a body. The body may itself be a MIME message that
	// consists of a header and body, etc. This method loads the contents of a file
	// into the body of a MIME message, without replacing the header.
	// 
	// The Content-Type and Content-Transfer-Encoding header fields are automatically
	// updated to match the type of content loaded (based on file extension). If your
	// application requires the MIME to have a specific Content-Type and/or
	// Content-Transfer-Encoding, set the ContentType and Encoding properties after
	// calling this method (not before).
	// 
	bool SetBodyFromFile(const char *fileName);

	// Sets the MIME message body from a string containing HTML. The Content-Type
	// header is added or updated to the value "text/html".
	// 
	// If 8bit (non-us-ascii) characters are present, and if the Charset property was
	// not previously set, then the "charset" attribute is automatically added to the
	// Content-Type header using the default value of "utf-8". This can be changed at
	// any time by setting the Charset property.
	// 
	// If the Encoding property was not previously set, then the
	// Content-Transfer-Encoding header is automatically added. It will be set to
	// "7bit" or "8bit" depending on whether the HTML body contains 8-bit non-us-ascii
	// characters.
	// 
	// To set the MIME body with no intentional side-effects, use SetBody instead.
	// 
	bool SetBodyFromHtml(const char *str);

	// Sets the MIME message body from a string containing plain-text. The Content-Type
	// header is added or updated to the value "text/plain".
	// 
	// If 8bit (non-us-ascii) characters are present, and if the Charset property was
	// not previously set, then the "charset" attribute is automatically added to the
	// Content-Type header using the default value of "utf-8". This can be changed at
	// any time by setting the Charset property.
	// 
	// If the Encoding property was not previously set, then the
	// Content-Transfer-Encoding header is automatically added. It will be set to
	// "7bit" or "8bit" depending on whether the plain-text body contains 8-bit
	// non-us-ascii characters.
	// 
	// To set the MIME body with no intentional side-effects, use SetBody instead.
	// 
	bool SetBodyFromPlainText(const char *str);

	// Sets the MIME message body from a string containing XML. The Content-Type header
	// is added or updated to the value "text/xml".
	// 
	// If 8bit (non-us-ascii) characters are present, and if the Charset property was
	// not previously set, then the "charset" attribute is automatically added to the
	// Content-Type header using the default value of "utf-8". This can be changed at
	// any time by setting the Charset property.
	// 
	// If the Encoding property was not previously set, then the
	// Content-Transfer-Encoding header is automatically added. It will be set to
	// "7bit" or "8bit" depending on whether the plain-text body contains 8-bit
	// non-us-ascii characters.
	// 
	// To set the MIME body with no intentional side-effects, use SetBody instead.
	// 
	bool SetBodyFromXml(const char *str);

#if defined(CK_CSP_INCLUDED)
	// (Only applies to the Microsoft Windows OS) Sets the Cryptographic Service
	// Provider (CSP) to be used for encryption / signing, or decryption / signature
	// verification.
	// 
	// This is not commonly used becaues the default Microsoft CSP is typically
	// appropriate. One instance where SetCSP is necessary is when using the Crypto-Pro
	// CSP for the GOST R 34.10-2001 and GOST R 34.10-94 providers.
	// 
	bool SetCSP(const CkCsp &csp);
#endif

	// Adds or replaces a MIME message header field. If the field already exists, it is
	// automatically replaced. Otherwise it is added. Pass zero-length  value to remove
	// the header field.
	bool SetHeaderField(const char *name, const char *value);

	// Allows a certificate to be explicitly specified for verifying a signature.
	bool SetVerifyCert(const CkCert &cert);

	// Unlocks the component allowing for the full functionality to be used. If this
	// method unexpectedly returns false, examine the contents of the LastErrorText
	// property to determine the reason for failure.
	bool UnlockComponent(const char *unlockCode);

	// Decrypts and/or verifies all digital signatures contained within the MIME
	// message, and returns true if all decryptions and verifications succeeded.
	// Otherwise returns false. After unwrapping, the information regarding security
	// and certificates can be obtained by the methods GetSignerCert and
	// GetEncryptCert, and the properties NumEncryptCerts and NumSignerCerts.
	// 
	// The MIME is restored to the original structure/content prior to all signing
	// and/or encryption.
	// 
	// The difference between UnwrapSecurity and methods such as Verify or Decrypt is
	// that UnwrapSecurity will recursively traverse the MIME to decrypt and/or verify
	// all parts. Also, UnwrapSecurity will unwrap layers until no further
	// encrypted/signed content is found. For example, if a MIME message was encrypted
	// and then subsequently signed, then UnwrapSecurity will verify and unwrap the
	// detached signature/signed-data layer, and then decrypt the "enveloped data".
	// 
	bool UnwrapSecurity(void);

	// URL encodes the MIME body. The charset is important. For example, consider this
	// MIME:
	// Content-Type: text/plain
	// Content-Transfer-Encoding: 8bit
	// 
	// SociÃ©tÃ©
	// If the charset is set to "utf-8", then the following is produced:
	// Content-Type: text/plain
	// Content-Transfer-Encoding: 8bit
	// 
	// SociÃ©tÃ©Soci%C3%A9t%C3%A9
	// However, if the charset is set to "ansi", then the following is the result:
	// Content-Type: text/plain
	// Content-Transfer-Encoding: 8bit
	// 
	// SocitSoci%E9t%E9
	void UrlEncodeBody(const char *charset);

	// Verifies PKCS7 signed MIME and "unwraps" the signature. The MIME is restored to
	// the original structure that it would have originally had prior to signing. The
	// Verify method works with both detached signatures, as well as opaque/attached
	// signatures.
	// 
	// A PKCS7 signature usually embeds both the signing certificate with its public
	// key. Therefore, it is usually possible to verify a signature without the need to
	// already have the certificate installed. If the signature does not embed the
	// certificate, the Verify method will automatically locate and use the certificate
	// if it was correctly pre-installed on the computer.
	// 
	bool Verify(void);

	// Returns the full certificate chain for the Nth certificate used to sign the MIME
	// message. Indexing begins at 0.
	// The caller is responsible for deleting the object returned by this method.
	CkCertChain *GetSignerCertChain(int index);

	// Adds an XML certificate vault to the object's internal list of sources to be
	// searched for certificates and private keys when encrypting/decrypting or
	// signing/verifying. Unlike the AddPfxSourceData and AddPfxSourceFile methods,
	// only a single XML certificate vault can be used. If UseCertVault is called
	// multiple times, only the last certificate vault will be used, as each call to
	// UseCertVault will replace the certificate vault provided in previous calls.
	bool UseCertVault(CkXmlCertVault &vault);

	// Decrypts PKCS7 encrypted MIME (also known as S/MIME) using a specific
	// certificate.
	bool DecryptUsingCert(CkCert &cert);

	// Makes a certificate available for decrypting if needed by methods that decrypt,
	// such as UnwrapSecurity. This method may be called multiple times to make more
	// than one certificate (and it's private key) available. Alternative methods for
	// making certificates available are UseCertVault, AddPfxSourceFile, and
	// AddPfxSourceData.
	bool AddDecryptCert(CkCert &cert);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
