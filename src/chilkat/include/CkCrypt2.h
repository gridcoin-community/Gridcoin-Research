// CkCrypt2.h: interface for the CkCrypt2 class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkCrypt2_H
#define _CkCrypt2_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;
class CkCert;
class CkCsp;
class CkPrivateKey;
class CkXmlCertVault;
class CkCertChain;
class CkBaseProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkCrypt2
class CK_VISIBLE_PUBLIC CkCrypt2  : public CkMultiByteBase
{
    private:
	CkBaseProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkCrypt2(const CkCrypt2 &);
	CkCrypt2 &operator=(const CkCrypt2 &);

    public:
	CkCrypt2(void);
	virtual ~CkCrypt2(void);

	static CkCrypt2 *createNew(void);
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
	// The block-size (in bytes) of the selected encryption algorithm. For example, if
	// the CryptAlgorithm property is set to "aes", the BlockSize property is
	// automatically set to 16. The block-size for the ARC4 streaming encryption
	// algorithm is 1.
	int get_BlockSize(void);

	// Applies to all methods that create PKCS7 signatures. To create a CAdES-BES
	// signature, set this property equal to true. The default value of this property
	// is false.
	bool get_CadesEnabled(void);
	// Applies to all methods that create PKCS7 signatures. To create a CAdES-BES
	// signature, set this property equal to true. The default value of this property
	// is false.
	void put_CadesEnabled(bool newVal);

	// Applies to all methods that create PKCS7 signatures. To create a CAdES-EPES
	// signature, set the CadesEnabled property = true, and also provide values for
	// each of the following properties: CadesSigPolicyHash, CadesSigPolicyId, and
	// CadesSigPolicyUri. For example (in pseudo-code):
	// cryptObj.CadesSigPolicyId = "2.16.76.1.7.1.1.1"
	// cryptObj.CadesSigPolicyUri = "http://politicas.icpbrasil.gov.br/PA_AD_RB.der"
	// cryptObj.CadesSigPolicyHash = "rySugyKaMhiMR8Y/o5yuU2A2bF0="
	// Note: Do NOT use the values above. They are only provided as an example to show
	// valid values. For example, the Policy ID is an OID. The Policy URI is a
	// typically a URL to a DER encoded policy file, and the Policy Hash is a base64
	// encoded hash.
	void get_CadesSigPolicyHash(CkString &str);
	// Applies to all methods that create PKCS7 signatures. To create a CAdES-EPES
	// signature, set the CadesEnabled property = true, and also provide values for
	// each of the following properties: CadesSigPolicyHash, CadesSigPolicyId, and
	// CadesSigPolicyUri. For example (in pseudo-code):
	// cryptObj.CadesSigPolicyId = "2.16.76.1.7.1.1.1"
	// cryptObj.CadesSigPolicyUri = "http://politicas.icpbrasil.gov.br/PA_AD_RB.der"
	// cryptObj.CadesSigPolicyHash = "rySugyKaMhiMR8Y/o5yuU2A2bF0="
	// Note: Do NOT use the values above. They are only provided as an example to show
	// valid values. For example, the Policy ID is an OID. The Policy URI is a
	// typically a URL to a DER encoded policy file, and the Policy Hash is a base64
	// encoded hash.
	const char *cadesSigPolicyHash(void);
	// Applies to all methods that create PKCS7 signatures. To create a CAdES-EPES
	// signature, set the CadesEnabled property = true, and also provide values for
	// each of the following properties: CadesSigPolicyHash, CadesSigPolicyId, and
	// CadesSigPolicyUri. For example (in pseudo-code):
	// cryptObj.CadesSigPolicyId = "2.16.76.1.7.1.1.1"
	// cryptObj.CadesSigPolicyUri = "http://politicas.icpbrasil.gov.br/PA_AD_RB.der"
	// cryptObj.CadesSigPolicyHash = "rySugyKaMhiMR8Y/o5yuU2A2bF0="
	// Note: Do NOT use the values above. They are only provided as an example to show
	// valid values. For example, the Policy ID is an OID. The Policy URI is a
	// typically a URL to a DER encoded policy file, and the Policy Hash is a base64
	// encoded hash.
	void put_CadesSigPolicyHash(const char *newVal);

	// See the description for the CadesSigPolicyHash property above.
	void get_CadesSigPolicyId(CkString &str);
	// See the description for the CadesSigPolicyHash property above.
	const char *cadesSigPolicyId(void);
	// See the description for the CadesSigPolicyHash property above.
	void put_CadesSigPolicyId(const char *newVal);

	// See the description for the CadesSigPolicyHash property above.
	void get_CadesSigPolicyUri(CkString &str);
	// See the description for the CadesSigPolicyHash property above.
	const char *cadesSigPolicyUri(void);
	// See the description for the CadesSigPolicyHash property above.
	void put_CadesSigPolicyUri(const char *newVal);

	// Controls the character encoding of the text encrypted, signed, hashed or
	// compressed. This property is relevant wherever strings are used as inputs or
	// outputs.
	// 
	// When working with strings, it is important to know the exact bytes that are
	// being encrypted/hashed/signed/compressed. This is critical when interoperating
	// with other systems. If your application is sending an encrypted string to
	// another system that will decrypt it, you will need to know the encoding of the
	// string that is expected on the receiving end (after decryption). If you pass
	// Unicode data (2 byte per character) to the encryptor, subsequent decryption will
	// reproduce the original Unicode. However, it may be that your program works with
	// Unicode strings, but the recipient of the encrypted data works with iso-8859-1
	// strings. In such a case, setting the Charset property to "iso-8859-1" causes the
	// character data to be automatically converted to the Charset before being
	// encrypted (or compressed, or hashed, or signed). The set of valid charsets is
	// listed below:
	// ANSI
	// us-ascii
	// unicode
	// unicodefffe
	// iso-8859-1
	// iso-8859-2
	// iso-8859-3
	// iso-8859-4
	// iso-8859-5
	// iso-8859-6
	// iso-8859-7
	// iso-8859-8
	// iso-8859-9
	// iso-8859-13
	// iso-8859-15
	// windows-874
	// windows-1250
	// windows-1251
	// windows-1252
	// windows-1253
	// windows-1254
	// windows-1255
	// windows-1256
	// windows-1257
	// windows-1258
	// utf-7
	// utf-8
	// utf-32
	// utf-32be
	// shift_jis
	// gb2312
	// ks_c_5601-1987
	// big5
	// iso-2022-jp
	// iso-2022-kr
	// euc-jp
	// euc-kr
	// macintosh
	// x-mac-japanese
	// x-mac-chinesetrad
	// x-mac-korean
	// x-mac-arabic
	// x-mac-hebrew
	// x-mac-greek
	// x-mac-cyrillic
	// x-mac-chinesesimp
	// x-mac-romanian
	// x-mac-ukrainian
	// x-mac-thai
	// x-mac-ce
	// x-mac-icelandic
	// x-mac-turkish
	// x-mac-croatian
	// asmo-708
	// dos-720
	// dos-862
	// ibm037
	// ibm437
	// ibm500
	// ibm737
	// ibm775
	// ibm850
	// ibm852
	// ibm855
	// ibm857
	// ibm00858
	// ibm860
	// ibm861
	// ibm863
	// ibm864
	// ibm865
	// cp866
	// ibm869
	// ibm870
	// cp875
	// koi8-r
	// koi8-u
	// 
	void get_Charset(CkString &str);
	// Controls the character encoding of the text encrypted, signed, hashed or
	// compressed. This property is relevant wherever strings are used as inputs or
	// outputs.
	// 
	// When working with strings, it is important to know the exact bytes that are
	// being encrypted/hashed/signed/compressed. This is critical when interoperating
	// with other systems. If your application is sending an encrypted string to
	// another system that will decrypt it, you will need to know the encoding of the
	// string that is expected on the receiving end (after decryption). If you pass
	// Unicode data (2 byte per character) to the encryptor, subsequent decryption will
	// reproduce the original Unicode. However, it may be that your program works with
	// Unicode strings, but the recipient of the encrypted data works with iso-8859-1
	// strings. In such a case, setting the Charset property to "iso-8859-1" causes the
	// character data to be automatically converted to the Charset before being
	// encrypted (or compressed, or hashed, or signed). The set of valid charsets is
	// listed below:
	// ANSI
	// us-ascii
	// unicode
	// unicodefffe
	// iso-8859-1
	// iso-8859-2
	// iso-8859-3
	// iso-8859-4
	// iso-8859-5
	// iso-8859-6
	// iso-8859-7
	// iso-8859-8
	// iso-8859-9
	// iso-8859-13
	// iso-8859-15
	// windows-874
	// windows-1250
	// windows-1251
	// windows-1252
	// windows-1253
	// windows-1254
	// windows-1255
	// windows-1256
	// windows-1257
	// windows-1258
	// utf-7
	// utf-8
	// utf-32
	// utf-32be
	// shift_jis
	// gb2312
	// ks_c_5601-1987
	// big5
	// iso-2022-jp
	// iso-2022-kr
	// euc-jp
	// euc-kr
	// macintosh
	// x-mac-japanese
	// x-mac-chinesetrad
	// x-mac-korean
	// x-mac-arabic
	// x-mac-hebrew
	// x-mac-greek
	// x-mac-cyrillic
	// x-mac-chinesesimp
	// x-mac-romanian
	// x-mac-ukrainian
	// x-mac-thai
	// x-mac-ce
	// x-mac-icelandic
	// x-mac-turkish
	// x-mac-croatian
	// asmo-708
	// dos-720
	// dos-862
	// ibm037
	// ibm437
	// ibm500
	// ibm737
	// ibm775
	// ibm850
	// ibm852
	// ibm855
	// ibm857
	// ibm00858
	// ibm860
	// ibm861
	// ibm863
	// ibm864
	// ibm865
	// cp866
	// ibm869
	// ibm870
	// cp875
	// koi8-r
	// koi8-u
	// 
	const char *charset(void);
	// Controls the character encoding of the text encrypted, signed, hashed or
	// compressed. This property is relevant wherever strings are used as inputs or
	// outputs.
	// 
	// When working with strings, it is important to know the exact bytes that are
	// being encrypted/hashed/signed/compressed. This is critical when interoperating
	// with other systems. If your application is sending an encrypted string to
	// another system that will decrypt it, you will need to know the encoding of the
	// string that is expected on the receiving end (after decryption). If you pass
	// Unicode data (2 byte per character) to the encryptor, subsequent decryption will
	// reproduce the original Unicode. However, it may be that your program works with
	// Unicode strings, but the recipient of the encrypted data works with iso-8859-1
	// strings. In such a case, setting the Charset property to "iso-8859-1" causes the
	// character data to be automatically converted to the Charset before being
	// encrypted (or compressed, or hashed, or signed). The set of valid charsets is
	// listed below:
	// ANSI
	// us-ascii
	// unicode
	// unicodefffe
	// iso-8859-1
	// iso-8859-2
	// iso-8859-3
	// iso-8859-4
	// iso-8859-5
	// iso-8859-6
	// iso-8859-7
	// iso-8859-8
	// iso-8859-9
	// iso-8859-13
	// iso-8859-15
	// windows-874
	// windows-1250
	// windows-1251
	// windows-1252
	// windows-1253
	// windows-1254
	// windows-1255
	// windows-1256
	// windows-1257
	// windows-1258
	// utf-7
	// utf-8
	// utf-32
	// utf-32be
	// shift_jis
	// gb2312
	// ks_c_5601-1987
	// big5
	// iso-2022-jp
	// iso-2022-kr
	// euc-jp
	// euc-kr
	// macintosh
	// x-mac-japanese
	// x-mac-chinesetrad
	// x-mac-korean
	// x-mac-arabic
	// x-mac-hebrew
	// x-mac-greek
	// x-mac-cyrillic
	// x-mac-chinesesimp
	// x-mac-romanian
	// x-mac-ukrainian
	// x-mac-thai
	// x-mac-ce
	// x-mac-icelandic
	// x-mac-turkish
	// x-mac-croatian
	// asmo-708
	// dos-720
	// dos-862
	// ibm037
	// ibm437
	// ibm500
	// ibm737
	// ibm775
	// ibm850
	// ibm852
	// ibm855
	// ibm857
	// ibm00858
	// ibm860
	// ibm861
	// ibm863
	// ibm864
	// ibm865
	// cp866
	// ibm869
	// ibm870
	// cp875
	// koi8-r
	// koi8-u
	// 
	void put_Charset(const char *newVal);

	// Controls the cipher mode for block encryption algorithms (AES, Blowfish,
	// TwoFish, DES, 3DES, RC2). Possible values are "CBC" (the default) , "ECB" and
	// "CFB".
	// 
	// CBC is an acronym for Cipher Block Chaining, ECB is an acronym for Electronic
	// CookBook, and CFB is for Cipher Feedback (see
	// http://en.wikipedia.org/wiki/Block_cipher_modes_of_operation )
	// 
	// Note: The CFB mode is currently only implemented for Blowfish and DES/3DES.
	// 
	void get_CipherMode(CkString &str);
	// Controls the cipher mode for block encryption algorithms (AES, Blowfish,
	// TwoFish, DES, 3DES, RC2). Possible values are "CBC" (the default) , "ECB" and
	// "CFB".
	// 
	// CBC is an acronym for Cipher Block Chaining, ECB is an acronym for Electronic
	// CookBook, and CFB is for Cipher Feedback (see
	// http://en.wikipedia.org/wiki/Block_cipher_modes_of_operation )
	// 
	// Note: The CFB mode is currently only implemented for Blowfish and DES/3DES.
	// 
	const char *cipherMode(void);
	// Controls the cipher mode for block encryption algorithms (AES, Blowfish,
	// TwoFish, DES, 3DES, RC2). Possible values are "CBC" (the default) , "ECB" and
	// "CFB".
	// 
	// CBC is an acronym for Cipher Block Chaining, ECB is an acronym for Electronic
	// CookBook, and CFB is for Cipher Feedback (see
	// http://en.wikipedia.org/wiki/Block_cipher_modes_of_operation )
	// 
	// Note: The CFB mode is currently only implemented for Blowfish and DES/3DES.
	// 
	void put_CipherMode(const char *newVal);

	// Selects the compression algorithm to be used for the various compress and
	// inflate methods. Currently, the only valid setting is "BZIP2".
	void get_CompressionAlgorithm(CkString &str);
	// Selects the compression algorithm to be used for the various compress and
	// inflate methods. Currently, the only valid setting is "BZIP2".
	const char *compressionAlgorithm(void);
	// Selects the compression algorithm to be used for the various compress and
	// inflate methods. Currently, the only valid setting is "BZIP2".
	void put_CompressionAlgorithm(const char *newVal);

	// Selects the encryption algorithm for encrypting and decrypting. Possible values
	// are: "pki", "aes", "blowfish", "blowfish2", "des", "3des", "rc2", "arc4",
	// "twofish", "pbes1" and "pbes2". The "pki" encryption algorithm isn't a specific
	// algorithm, but instead tells the component to encrypt/decrypt using public-key
	// encryption with digital certificates. The other choices are symmetric encryption
	// algorithms that do not involve digital certificates and public/private keys.
	// 
	// The original Chilkat implementation of Blowfish has a 4321 byte-swapping issue
	// (the results are 4321 byte-swapped). The new implementation ("blowfish2") does
	// not byte swap. This should be used for compatibility with other Blowfish
	// software.
	// 
	// Password-based encryption (PBE) is selected by setting this property to "pbes1"
	// or "pbes2". Password-based encryption is defined in the PKCS5 Password-Based
	// Cryptography Standard at http://www.rsa.com/rsalabs/node.asp?id=2127. If PBE is
	// used, the underlying encryption algorithm is specified by the PbesAlgorithm
	// property. The underlying encryption (PbesAlgorithm) for PBES1 is limited to
	// 56-bit DES or 64-bit RC2.
	// 
	void get_CryptAlgorithm(CkString &str);
	// Selects the encryption algorithm for encrypting and decrypting. Possible values
	// are: "pki", "aes", "blowfish", "blowfish2", "des", "3des", "rc2", "arc4",
	// "twofish", "pbes1" and "pbes2". The "pki" encryption algorithm isn't a specific
	// algorithm, but instead tells the component to encrypt/decrypt using public-key
	// encryption with digital certificates. The other choices are symmetric encryption
	// algorithms that do not involve digital certificates and public/private keys.
	// 
	// The original Chilkat implementation of Blowfish has a 4321 byte-swapping issue
	// (the results are 4321 byte-swapped). The new implementation ("blowfish2") does
	// not byte swap. This should be used for compatibility with other Blowfish
	// software.
	// 
	// Password-based encryption (PBE) is selected by setting this property to "pbes1"
	// or "pbes2". Password-based encryption is defined in the PKCS5 Password-Based
	// Cryptography Standard at http://www.rsa.com/rsalabs/node.asp?id=2127. If PBE is
	// used, the underlying encryption algorithm is specified by the PbesAlgorithm
	// property. The underlying encryption (PbesAlgorithm) for PBES1 is limited to
	// 56-bit DES or 64-bit RC2.
	// 
	const char *cryptAlgorithm(void);
	// Selects the encryption algorithm for encrypting and decrypting. Possible values
	// are: "pki", "aes", "blowfish", "blowfish2", "des", "3des", "rc2", "arc4",
	// "twofish", "pbes1" and "pbes2". The "pki" encryption algorithm isn't a specific
	// algorithm, but instead tells the component to encrypt/decrypt using public-key
	// encryption with digital certificates. The other choices are symmetric encryption
	// algorithms that do not involve digital certificates and public/private keys.
	// 
	// The original Chilkat implementation of Blowfish has a 4321 byte-swapping issue
	// (the results are 4321 byte-swapped). The new implementation ("blowfish2") does
	// not byte swap. This should be used for compatibility with other Blowfish
	// software.
	// 
	// Password-based encryption (PBE) is selected by setting this property to "pbes1"
	// or "pbes2". Password-based encryption is defined in the PKCS5 Password-Based
	// Cryptography Standard at http://www.rsa.com/rsalabs/node.asp?id=2127. If PBE is
	// used, the underlying encryption algorithm is specified by the PbesAlgorithm
	// property. The underlying encryption (PbesAlgorithm) for PBES1 is limited to
	// 56-bit DES or 64-bit RC2.
	// 
	void put_CryptAlgorithm(const char *newVal);

	// Controls the encoding of binary data to a printable string for many methods. The
	// valid modes are "Base64", "modBase64", "Base32", "UU", "QP" (for
	// quoted-printable), "URL" (for url-encoding), "Hex", "Q", "B", "url_oath",
	// "url_rfc1738", "url_rfc2396", and "url_rfc3986".
	void get_EncodingMode(CkString &str);
	// Controls the encoding of binary data to a printable string for many methods. The
	// valid modes are "Base64", "modBase64", "Base32", "UU", "QP" (for
	// quoted-printable), "URL" (for url-encoding), "Hex", "Q", "B", "url_oath",
	// "url_rfc1738", "url_rfc2396", and "url_rfc3986".
	const char *encodingMode(void);
	// Controls the encoding of binary data to a printable string for many methods. The
	// valid modes are "Base64", "modBase64", "Base32", "UU", "QP" (for
	// quoted-printable), "URL" (for url-encoding), "Hex", "Q", "B", "url_oath",
	// "url_rfc1738", "url_rfc2396", and "url_rfc3986".
	void put_EncodingMode(const char *newVal);

	// Chilkat Crypt2 provides the ability to feed the encryption/decryption methods
	// with chunks of data. This allows a large amount of data, or a data stream, to be
	// fed piecemeal for encrypting or decrypting. It applies to all symmetric
	// algorithms currently supported (AES, Blowfish, Twofish, 3DES, RC2, DES, ARC4),
	// and all algorithms supported in the future.
	// 
	// The default value for both FirstChunk and LastChunk is true "” meaning that
	// when you call an Encrypt* or Decrypt* method, it is both the first and last
	// chunk (i.e. it"™s the entire amount of data to be encrypted or decrypted).
	// 
	// If you wish to feed the data piecemeal, do this:
	// 
	//     Set FirstChunk = true, LastChunk = false for the first chunk of data.
	//     For all "middle" chunks (i.e. all chunks except for the final chunk) set
	//     FirstChunk = false and LastChunk = false.
	//     For the final chunk, set FirstChunk = false and LastChunk = true
	// 
	// You don"™t need to worry about feeding data according to the block size of the
	// encryption algorithm. For example, AES has a block size of 16 bytes. You may
	// feed data in chunks of any size. The Chilkat Crypt2 component will buffer the
	// data. When the final chunk is passed, the output is padded to the algorithm"™s
	// block size according to the PaddingScheme.
	// 
	bool get_FirstChunk(void);
	// Chilkat Crypt2 provides the ability to feed the encryption/decryption methods
	// with chunks of data. This allows a large amount of data, or a data stream, to be
	// fed piecemeal for encrypting or decrypting. It applies to all symmetric
	// algorithms currently supported (AES, Blowfish, Twofish, 3DES, RC2, DES, ARC4),
	// and all algorithms supported in the future.
	// 
	// The default value for both FirstChunk and LastChunk is true "” meaning that
	// when you call an Encrypt* or Decrypt* method, it is both the first and last
	// chunk (i.e. it"™s the entire amount of data to be encrypted or decrypted).
	// 
	// If you wish to feed the data piecemeal, do this:
	// 
	//     Set FirstChunk = true, LastChunk = false for the first chunk of data.
	//     For all "middle" chunks (i.e. all chunks except for the final chunk) set
	//     FirstChunk = false and LastChunk = false.
	//     For the final chunk, set FirstChunk = false and LastChunk = true
	// 
	// You don"™t need to worry about feeding data according to the block size of the
	// encryption algorithm. For example, AES has a block size of 16 bytes. You may
	// feed data in chunks of any size. The Chilkat Crypt2 component will buffer the
	// data. When the final chunk is passed, the output is padded to the algorithm"™s
	// block size according to the PaddingScheme.
	// 
	void put_FirstChunk(bool newVal);

	// Selects the hash algorithm used by methods that create hashes. The valid choices
	// are "sha1", "sha256", "sha384", "sha512", "md2", "md5", "haval", "ripemd128",
	// "ripemd160","ripemd256", or "ripemd320".
	void get_HashAlgorithm(CkString &str);
	// Selects the hash algorithm used by methods that create hashes. The valid choices
	// are "sha1", "sha256", "sha384", "sha512", "md2", "md5", "haval", "ripemd128",
	// "ripemd160","ripemd256", or "ripemd320".
	const char *hashAlgorithm(void);
	// Selects the hash algorithm used by methods that create hashes. The valid choices
	// are "sha1", "sha256", "sha384", "sha512", "md2", "md5", "haval", "ripemd128",
	// "ripemd160","ripemd256", or "ripemd320".
	void put_HashAlgorithm(const char *newVal);

	// Applies to the HAVAL hash algorithm only and must be set to the integer value 3,
	// 4, or 5. The default value is 3.
	int get_HavalRounds(void);
	// Applies to the HAVAL hash algorithm only and must be set to the integer value 3,
	// 4, or 5. The default value is 3.
	void put_HavalRounds(int newVal);

	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort some methods call prior to
	// completion. If HeartbeatMs is 0 (the default), no AbortCheck event callbacks
	// will fire.
	// 
	// The methods with event callbacks are: CkDecryptFile, CkEncryptFile, HashFile,
	// and HashFileENC.
	// 
	int get_HeartbeatMs(void);
	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort some methods call prior to
	// completion. If HeartbeatMs is 0 (the default), no AbortCheck event callbacks
	// will fire.
	// 
	// The methods with event callbacks are: CkDecryptFile, CkEncryptFile, HashFile,
	// and HashFileENC.
	// 
	void put_HeartbeatMs(int newVal);

	// The initialization vector to be used with symmetric encryption algorithms
	// (Rijndael, Blowfish, Twofish). If left unset, no initialization vector is used.
	void get_IV(CkByteData &outBytes);
	// The initialization vector to be used with symmetric encryption algorithms
	// (Rijndael, Blowfish, Twofish). If left unset, no initialization vector is used.
	void put_IV(const CkByteData &inBytes);

	// Only applies when creating digital signatures. If true (the default), then
	// additional certificates (if any) in the chain of authentication are included in
	// the PKCS7 digital signature.
	bool get_IncludeCertChain(void);
	// Only applies when creating digital signatures. If true (the default), then
	// additional certificates (if any) in the chain of authentication are included in
	// the PKCS7 digital signature.
	void put_IncludeCertChain(bool newVal);

	// Iteration count to be used with password-based encryption (PBE). Password-based
	// encryption is defined in the PKCS5 Password-Based Cryptography Standard at
	// http://www.rsa.com/rsalabs/node.asp?id=2127
	// 
	// The purpose of the iteration count is to increase the computation required to
	// encrypt and decrypt. A larger iteration count makes cracking via exhaustive
	// search more difficult. The default value is 1024.
	// 
	int get_IterationCount(void);
	// Iteration count to be used with password-based encryption (PBE). Password-based
	// encryption is defined in the PKCS5 Password-Based Cryptography Standard at
	// http://www.rsa.com/rsalabs/node.asp?id=2127
	// 
	// The purpose of the iteration count is to increase the computation required to
	// encrypt and decrypt. A larger iteration count makes cracking via exhaustive
	// search more difficult. The default value is 1024.
	// 
	void put_IterationCount(int newVal);

	// The key length in bits for symmetric encryption algorithms. The default value is
	// 128.
	int get_KeyLength(void);
	// The key length in bits for symmetric encryption algorithms. The default value is
	// 128.
	void put_KeyLength(int newVal);

	// (See the description for the FirstChunk property.)
	bool get_LastChunk(void);
	// (See the description for the FirstChunk property.)
	void put_LastChunk(bool newVal);

	// This property is set when a digital signature is verified. It contains the
	// number of signer certificates. Each signing certificate can be retrieved by
	// calling the GetSignerCert method, passing an index from 0 to NumSignerCerts-1.
	int get_NumSignerCerts(void);

	// The padding scheme used by block encryption algorithms such as AES (Rijndael),
	// Blowfish, Twofish, RC2, DES, 3DES, etc. Block encryption algorithms pad
	// encrypted data to a multiple of algorithm's block size. The default value of
	// this property is 0.
	// 
	// Possible values are:
	// 
	// 0 = RFC 1423 padding scheme: Each padding byte is set to the number of padding
	// bytes. If the data is already a multiple of algorithm's block size bytes, an
	// extra block is appended each having a value equal to the block size. (for
	// example, if the algorithm's block size is 16, then 16 bytes having the value
	// 0x10 are added.). (This is also known as PKCS5 padding: PKCS #5 padding string
	// consists of a sequence of bytes, each of which is equal to the total number of
	// padding bytes added. )
	// 
	// 1 = FIPS81 (Federal Information Processing Standards 81) where the last byte
	// contains the number of padding bytes, including itself, and the other padding
	// bytes are set to random values.
	// 
	// 2 = Each padding byte is set to a random value. The decryptor must know how many
	// bytes are in the original unencrypted data.
	// 
	// 3 = Pad with NULLs. (If already a multiple of the algorithm's block size, no
	// padding is added).
	// 
	// 4 = Pad with SPACE chars(0x20). (If already a multiple of algorithm's block
	// size, no padding is added).
	// 
	int get_PaddingScheme(void);
	// The padding scheme used by block encryption algorithms such as AES (Rijndael),
	// Blowfish, Twofish, RC2, DES, 3DES, etc. Block encryption algorithms pad
	// encrypted data to a multiple of algorithm's block size. The default value of
	// this property is 0.
	// 
	// Possible values are:
	// 
	// 0 = RFC 1423 padding scheme: Each padding byte is set to the number of padding
	// bytes. If the data is already a multiple of algorithm's block size bytes, an
	// extra block is appended each having a value equal to the block size. (for
	// example, if the algorithm's block size is 16, then 16 bytes having the value
	// 0x10 are added.). (This is also known as PKCS5 padding: PKCS #5 padding string
	// consists of a sequence of bytes, each of which is equal to the total number of
	// padding bytes added. )
	// 
	// 1 = FIPS81 (Federal Information Processing Standards 81) where the last byte
	// contains the number of padding bytes, including itself, and the other padding
	// bytes are set to random values.
	// 
	// 2 = Each padding byte is set to a random value. The decryptor must know how many
	// bytes are in the original unencrypted data.
	// 
	// 3 = Pad with NULLs. (If already a multiple of the algorithm's block size, no
	// padding is added).
	// 
	// 4 = Pad with SPACE chars(0x20). (If already a multiple of algorithm's block
	// size, no padding is added).
	// 
	void put_PaddingScheme(int newVal);

	// If the CryptAlgorithm property is set to "pbes1" or "pbes2", this property
	// specifies the underlying encryption algorithm to be used with password-based
	// encryption (PBE). Password-based encryption is defined in the PKCS5
	// Password-Based Cryptography Standard at
	// http://www.rsa.com/rsalabs/node.asp?id=2127
	void get_PbesAlgorithm(CkString &str);
	// If the CryptAlgorithm property is set to "pbes1" or "pbes2", this property
	// specifies the underlying encryption algorithm to be used with password-based
	// encryption (PBE). Password-based encryption is defined in the PKCS5
	// Password-Based Cryptography Standard at
	// http://www.rsa.com/rsalabs/node.asp?id=2127
	const char *pbesAlgorithm(void);
	// If the CryptAlgorithm property is set to "pbes1" or "pbes2", this property
	// specifies the underlying encryption algorithm to be used with password-based
	// encryption (PBE). Password-based encryption is defined in the PKCS5
	// Password-Based Cryptography Standard at
	// http://www.rsa.com/rsalabs/node.asp?id=2127
	void put_PbesAlgorithm(const char *newVal);

	// The password to be used with password-based encryption (PBE). Password-based
	// encryption is defined in the PKCS5 Password-Based Cryptography Standard at
	// http://www.rsa.com/rsalabs/node.asp?id=2127
	void get_PbesPassword(CkString &str);
	// The password to be used with password-based encryption (PBE). Password-based
	// encryption is defined in the PKCS5 Password-Based Cryptography Standard at
	// http://www.rsa.com/rsalabs/node.asp?id=2127
	const char *pbesPassword(void);
	// The password to be used with password-based encryption (PBE). Password-based
	// encryption is defined in the PKCS5 Password-Based Cryptography Standard at
	// http://www.rsa.com/rsalabs/node.asp?id=2127
	void put_PbesPassword(const char *newVal);

	// When the CryptAlgorithm property is "PKI" to select PKCS7 public-key encryption,
	// this selects the underlying symmetric encryption algorithm. Possible values are:
	// "aes", "des", "3des", and "rc2".
	void get_Pkcs7CryptAlg(CkString &str);
	// When the CryptAlgorithm property is "PKI" to select PKCS7 public-key encryption,
	// this selects the underlying symmetric encryption algorithm. Possible values are:
	// "aes", "des", "3des", and "rc2".
	const char *pkcs7CryptAlg(void);
	// When the CryptAlgorithm property is "PKI" to select PKCS7 public-key encryption,
	// this selects the underlying symmetric encryption algorithm. Possible values are:
	// "aes", "des", "3des", and "rc2".
	void put_Pkcs7CryptAlg(const char *newVal);

	// The effective key length (in bits) for the RC2 encryption algorithm. When RC2 is
	// used, both the KeyLength and Rc2EffectiveKeyLength properties should be set. For
	// RC2, both should be between 8 and 1024 (inclusive).
	int get_Rc2EffectiveKeyLength(void);
	// The effective key length (in bits) for the RC2 encryption algorithm. When RC2 is
	// used, both the KeyLength and Rc2EffectiveKeyLength properties should be set. For
	// RC2, both should be between 8 and 1024 (inclusive).
	void put_Rc2EffectiveKeyLength(int newVal);

	// The salt to be used with password-based encryption (PBE). Password-based
	// encryption is defined in the PKCS5 Password-Based Cryptography Standard at
	// http://www.rsa.com/rsalabs/node.asp?id=2127
	void get_Salt(CkByteData &outBytes);
	// The salt to be used with password-based encryption (PBE). Password-based
	// encryption is defined in the PKCS5 Password-Based Cryptography Standard at
	// http://www.rsa.com/rsalabs/node.asp?id=2127
	void put_Salt(const CkByteData &inBytes);

	// The binary secret key used for symmetric encryption (Rijndael, Blowfish,
	// Twofish). The secret key must be identical for decryption to succeed. The length
	// in bytes of the SecretKey must equal the KeyLength/8.
	void get_SecretKey(CkByteData &outBytes);
	// The binary secret key used for symmetric encryption (Rijndael, Blowfish,
	// Twofish). The secret key must be identical for decryption to succeed. The length
	// in bytes of the SecretKey must equal the KeyLength/8.
	void put_SecretKey(const CkByteData &inBytes);

	// When UU encoding, this is the filename to be embedded in UU encoded output. The
	// default is "file.dat". When UU decoding, this is the filename found in the UU
	// encoded input.
	void get_UuFilename(CkString &str);
	// When UU encoding, this is the filename to be embedded in UU encoded output. The
	// default is "file.dat". When UU decoding, this is the filename found in the UU
	// encoded input.
	const char *uuFilename(void);
	// When UU encoding, this is the filename to be embedded in UU encoded output. The
	// default is "file.dat". When UU decoding, this is the filename found in the UU
	// encoded input.
	void put_UuFilename(const char *newVal);

	// When UU encoding, this is the file permissions mode to be embedded in UU encoded
	// output. The default is "644". When UU decoding, this property is set to the mode
	// found in the UU encoded input.
	void get_UuMode(CkString &str);
	// When UU encoding, this is the file permissions mode to be embedded in UU encoded
	// output. The default is "644". When UU decoding, this property is set to the mode
	// found in the UU encoded input.
	const char *uuMode(void);
	// When UU encoding, this is the file permissions mode to be embedded in UU encoded
	// output. The default is "644". When UU decoding, this property is set to the mode
	// found in the UU encoded input.
	void put_UuMode(const char *newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Adds a certificate to be used for public-key encryption. (To use public-key
	// encryption with digital certificates, set the CryptAlgorithm property = "pki".)
	// To encrypt with more than one certificate, call AddEncryptCert once per
	// certificate.
	void AddEncryptCert(CkCert &cert);

	// Adds a PFX to the object's internal list of sources to be searched for
	// certificates and private keys when decrypting. Multiple PFX sources can be added
	// by calling this method once for each. (On the Windows operating system, the
	// registry-based certificate stores are also automatically searched, so it is
	// commonly not required to explicitly add PFX sources.)
	// 
	// The pfxBytes contains the bytes of a PFX file (also known as PKCS12 or .p12).
	// 
	bool AddPfxSourceData(const CkByteData &pfxData, const char *password);

	// Adds a PFX file to the object's internal list of sources to be searched for
	// certificates and private keys when decrypting. Multiple PFX files can be added
	// by calling this method once for each. (On the Windows operating system, the
	// registry-based certificate stores are also automatically searched, so it is
	// commonly not required to explicitly add PFX sources.)
	// 
	// The pfxFilePath contains the bytes of a PFX file (also known as PKCS12 or .p12).
	// 
	bool AddPfxSourceFile(const char *pfxFilePath, const char *password);

	// Convenience method for byte swapping between little-endian byte ordering and
	// big-endian byte ordering.
	bool ByteSwap4321(const CkByteData &data, CkByteData &outBytes);

	// Utility method to convert bytes to a string -- interpreting the bytes according
	// to the charset specified.
	bool BytesToString(const CkByteData &inData, const char *charset, CkString &outStr);
	// Utility method to convert bytes to a string -- interpreting the bytes according
	// to the charset specified.
	const char *bytesToString(const CkByteData &inData, const char *charset);

	// File-to-file decryption. There is no limit to the size of the file that can be
	// decrypted because the component will operate in streaming mode internally.
	bool CkDecryptFile(const char *srcFile, const char *destFile);

	// File-to-file encryption. There is no limit to the size of the file that can be
	// encrypted because the component will operate in streaming mode internally.
	bool CkEncryptFile(const char *srcFile, const char *destFile);

	// Clears the internal list of digital certificates to be used for public-key
	// encryption.
	void ClearEncryptCerts(void);

	// Memory-to-memory compression. Compresses a byte array and returns a byte array
	// of compressed data. The compression algorithm specified by the
	// CompressionAlgorithm property is used. Currently, the only choice is "BZIP2".
	bool CompressBytes(const CkByteData &data, CkByteData &outData);

	// Same as CompressBytes, except an encoded string is returned. The encoding is
	// controlled by the EncodingMode property, which can be set to "Base64", "QP" (for
	// quoted-printable), or "Hex".
	bool CompressBytesENC(const CkByteData &data, CkString &outStr);
	// Same as CompressBytes, except an encoded string is returned. The encoding is
	// controlled by the EncodingMode property, which can be set to "Base64", "QP" (for
	// quoted-printable), or "Hex".
	const char *compressBytesENC(const CkByteData &data);

	// Compresses a string and returns a byte array of the compressed data. For
	// languages such as C#, VB.NET, Visual Basic 6, etc. the string input argument is
	// Unicode. The Charset property controls the conversion of the Unicode string to a
	// multibyte string before compression is applied. For example, if Charset is set
	// to "iso-8859-1", then the input string argument is first converted from Unicode
	// (2 bytes per char) to iso-8859-1 (1 byte per char) before compressing according
	// to the CompressionAlgorithm property ("BZIP2"). If the Charset property is set
	// to "unicode", then no character encoding conversion will happen, and the full
	// Unicode string is compressed.
	bool CompressString(const char *str, CkByteData &outData);

	// Compresses a string and returns an encoded string of the compressed data. For
	// languages such as C#, VB.NET, Visual Basic 6, etc. the string input argument is
	// Unicode. The Charset property controls the conversion of the Unicode string to a
	// multibyte string before compression is applied. For example, if Charset is set
	// to "iso-8859-1", then the input string argument is first converted from Unicode
	// (2 bytes per char) to iso-8859-1 (1 byte per char) before compressing according
	// to the CompressionAlgorithm property ("BZIP2"). If the Charset property is set
	// to "unicode", then no character encoding conversion will happen, and the full
	// Unicode string is compressed.
	// 
	// Compressed data is typically binary data which is not a printable string. This
	// method encodes the output compressed data to a printable string according to the
	// EncodingMode property, which can be set to "Base64", "QP" (for
	// quoted-printable), or "Hex".
	// 
	bool CompressStringENC(const char *str, CkString &outStr);
	// Compresses a string and returns an encoded string of the compressed data. For
	// languages such as C#, VB.NET, Visual Basic 6, etc. the string input argument is
	// Unicode. The Charset property controls the conversion of the Unicode string to a
	// multibyte string before compression is applied. For example, if Charset is set
	// to "iso-8859-1", then the input string argument is first converted from Unicode
	// (2 bytes per char) to iso-8859-1 (1 byte per char) before compressing according
	// to the CompressionAlgorithm property ("BZIP2"). If the Charset property is set
	// to "unicode", then no character encoding conversion will happen, and the full
	// Unicode string is compressed.
	// 
	// Compressed data is typically binary data which is not a printable string. This
	// method encodes the output compressed data to a printable string according to the
	// EncodingMode property, which can be set to "Base64", "QP" (for
	// quoted-printable), or "Hex".
	// 
	const char *compressStringENC(const char *str);

	// Digitally signs a file and writes the digital signature to a separate output
	// file (a PKCS#7 signature file). The input file (inFilePath) is unmodified. A
	// certificate for signing must be specified by calling SetSigningCert or
	// SetSigningCert2 prior to calling this method.
	// 
	// This method is equivalent to CreateP7S. The CreateP7S method was added to
	// clarify the format of the signature file that is created.
	// 
	bool CreateDetachedSignature(const char *filename, const char *sigFile);

	// Digitally signs a file and creates a .p7m (PKCS #7 Message) file that contains
	// both the signature and original file content. The input file (ARG1) is
	// unmodified. A certificate for signing must be specified by calling
	// SetSigningCert or SetSigningCert2 prior to calling this method.
	bool CreateP7M(const char *inFilename, const char *p7mFilename);

	// Digitally signs a file and creates a .p7s (PKCS #7 Signature) signature file.
	// The input file (inFilename) is unmodified. The output file ( outP7sFilename) contains only the
	// signature and not the original data. A certificate for signing must be specified
	// by calling SetSigningCert or SetSigningCert2 prior to calling this method.
	bool CreateP7S(const char *inFilename, const char *p7sFilename);

	// Decode binary data from an encoded string. The  encoding can be set to any of the
	// following strings: "base64", "hex", "quoted-printable", "url", "base32", "Q",
	// "B", "url_rc1738", "url_rfc2396", "url_rfc3986", "url_oauth", "uu", "modBase64",
	// or "html" (for HTML entity encoding).
	bool Decode(const char *str, const char *encoding, CkByteData &outData);

	// Decodes from an encoding back to the original string. The  encoding can be set to any
	// of the following strings: "base64", "hex", "quoted-printable", "url", "base32",
	// "Q", "B", "url_rc1738", "url_rfc2396", "url_rfc3986", "url_oauth", "uu",
	// "modBase64", or "html" (for HTML entity encoding).
	bool DecodeString(const char *inStr, const char *charset, const char *encoding, CkString &outStr);
	// Decodes from an encoding back to the original string. The  encoding can be set to any
	// of the following strings: "base64", "hex", "quoted-printable", "url", "base32",
	// "Q", "B", "url_rc1738", "url_rfc2396", "url_rfc3986", "url_oauth", "uu",
	// "modBase64", or "html" (for HTML entity encoding).
	const char *decodeString(const char *inStr, const char *charset, const char *encoding);

	// Decrypts a byte array and returns the unencrypted byte array. The property
	// settings used when encrypting the data must match the settings when decrypting.
	// Specifically, the CryptAlgorithm, CipherMode, PaddingScheme, KeyLength, IV, and
	// SecretKey properties must match.
	bool DecryptBytes(const CkByteData &data, CkByteData &outData);

	// Decrypts string-encoded encrypted data and returns the unencrypted byte array.
	// Data encrypted with EncryptBytesENC can be decrypted with this method. The
	// property settings used when encrypting the data must match the settings when
	// decrypting. Specifically, the EncodingMode, CryptAlgorithm, CipherMode,
	// PaddingScheme, KeyLength, IV, and SecretKey properties must match.
	bool DecryptBytesENC(const char *str, CkByteData &outData);

	// Encrypted data is passed to this method as an encoded string (base64, hex,
	// etc.). This method first decodes the input data according to the EncodingMode
	// property setting. It then decrypts and re-encodes using the EncodingMode
	// setting, and returns the decrypted data in encoded string form.
	bool DecryptEncoded(const char *str, CkString &outStr);
	// Encrypted data is passed to this method as an encoded string (base64, hex,
	// etc.). This method first decodes the input data according to the EncodingMode
	// property setting. It then decrypts and re-encodes using the EncodingMode
	// setting, and returns the decrypted data in encoded string form.
	const char *decryptEncoded(const char *str);

	// The reverse of EncryptString.
	// 
	// Decrypts encrypted byte data and returns the original string. The property
	// settings used when encrypting the string must match the settings when
	// decrypting. Specifically, the Charset, CryptAlgorithm, CipherMode,
	// PaddingScheme, KeyLength, IV, and SecretKey properties must match.
	// 
	bool DecryptString(const CkByteData &data, CkString &outStr);
	// The reverse of EncryptString.
	// 
	// Decrypts encrypted byte data and returns the original string. The property
	// settings used when encrypting the string must match the settings when
	// decrypting. Specifically, the Charset, CryptAlgorithm, CipherMode,
	// PaddingScheme, KeyLength, IV, and SecretKey properties must match.
	// 
	const char *decryptString(const CkByteData &data);

	// The reverse of EncryptStringENC.
	// 
	// Decrypts string-encoded encrypted data and returns the original string. The
	// property settings used when encrypting the string must match the settings when
	// decrypting. Specifically, the Charset, EncodingMode, CryptAlgorithm, CipherMode,
	// PaddingScheme, KeyLength, IV, and SecretKey properties must match.
	// 
	bool DecryptStringENC(const char *str, CkString &outStr);
	// The reverse of EncryptStringENC.
	// 
	// Decrypts string-encoded encrypted data and returns the original string. The
	// property settings used when encrypting the string must match the settings when
	// decrypting. Specifically, the Charset, EncodingMode, CryptAlgorithm, CipherMode,
	// PaddingScheme, KeyLength, IV, and SecretKey properties must match.
	// 
	const char *decryptStringENC(const char *str);

	// Encode binary data to base64, hex, quoted-printable, or URL-encoding. The  encoding
	// can be set to any of the following strings: "base64", "hex", "quoted-printable",
	// "url", "base32", "Q", "B", "url_rc1738", "url_rfc2396", "url_rfc3986",
	// "url_oauth", "uu", "modBase64", or "html" (for HTML entity encoding).
	bool Encode(const CkByteData &data, const char *encoding, CkString &outStr);
	// Encode binary data to base64, hex, quoted-printable, or URL-encoding. The  encoding
	// can be set to any of the following strings: "base64", "hex", "quoted-printable",
	// "url", "base32", "Q", "B", "url_rc1738", "url_rfc2396", "url_rfc3986",
	// "url_oauth", "uu", "modBase64", or "html" (for HTML entity encoding).
	const char *encode(const CkByteData &data, const char *encoding);

	// Encode binary data to base64, hex, quoted-printable, or URL-encoding. The  encoding
	// can be set to any of the following strings: "base64", "hex", "quoted-printable",
	// "url", "base32", "Q", "B", "url_rc1738", "url_rfc2396", "url_rfc3986",
	// "url_oauth", "uu", "modBase64", or "html" (for HTML entity encoding).
	// 
	// The pByteData points to the bytes to be encoded. The  szByteData specifies the number of
	// bytes to encode.
	// 
	bool EncodeBytes(const void *pByteData, unsigned long szByteData, const char *encoding, CkString &outStr);
	// Encode binary data to base64, hex, quoted-printable, or URL-encoding. The  encoding
	// can be set to any of the following strings: "base64", "hex", "quoted-printable",
	// "url", "base32", "Q", "B", "url_rc1738", "url_rfc2396", "url_rfc3986",
	// "url_oauth", "uu", "modBase64", or "html" (for HTML entity encoding).
	// 
	// The pByteData points to the bytes to be encoded. The  szByteData specifies the number of
	// bytes to encode.
	// 
	const char *encodeBytes(const void *pByteData, unsigned long szByteData, const char *encoding);

	// Encodes a string. The  toEncodingName can be set to any of the following strings: "base64",
	// "hex", "quoted-printable", "url", "base32", "Q", "B", "url_rc1738",
	// "url_rfc2396", "url_rfc3986", "url_oauth", "uu", "modBase64", or "html" (for
	// HTML entity encoding). The  charsetName is important, and usually you'll want to specify
	// "ansi". For example, if the string "ABC" is to be encoded to "hex" using ANSI,
	// the result will be "414243". However, if "unicode" is used, the result is
	// "410042004300".
	bool EncodeString(const char *inStr, const char *charset, const char *encoding, CkString &outStr);
	// Encodes a string. The  toEncodingName can be set to any of the following strings: "base64",
	// "hex", "quoted-printable", "url", "base32", "Q", "B", "url_rc1738",
	// "url_rfc2396", "url_rfc3986", "url_oauth", "uu", "modBase64", or "html" (for
	// HTML entity encoding). The  charsetName is important, and usually you'll want to specify
	// "ansi". For example, if the string "ABC" is to be encoded to "hex" using ANSI,
	// the result will be "414243". However, if "unicode" is used, the result is
	// "410042004300".
	const char *encodeString(const char *inStr, const char *charset, const char *encoding);

	// Encrypts a byte array. The minimal set of properties that should be set before
	// encrypting are: CryptAlgorithm, SecretKey. Other properties that control
	// encryption are: CipherMode, PaddingScheme, KeyLength, IV. When decrypting, all
	// property settings must match otherwise garbled data is returned.
	bool EncryptBytes(const CkByteData &data, CkByteData &outData);

	// Encrypts a byte array and returns the encrypted data as an encoded (printable)
	// string. The minimal set of properties that should be set before encrypting are:
	// CryptAlgorithm, SecretKey, EncodingMode. Other properties that control
	// encryption are: CipherMode, PaddingScheme, KeyLength, IV. When decrypting, all
	// property settings must match otherwise garbled data is returned. The encoding of
	// the string that is returned is controlled by the EncodingMode property, which
	// can be set to "Base64", "QP", or "Hex".
	bool EncryptBytesENC(const CkByteData &data, CkString &outStr);
	// Encrypts a byte array and returns the encrypted data as an encoded (printable)
	// string. The minimal set of properties that should be set before encrypting are:
	// CryptAlgorithm, SecretKey, EncodingMode. Other properties that control
	// encryption are: CipherMode, PaddingScheme, KeyLength, IV. When decrypting, all
	// property settings must match otherwise garbled data is returned. The encoding of
	// the string that is returned is controlled by the EncodingMode property, which
	// can be set to "Base64", "QP", or "Hex".
	const char *encryptBytesENC(const CkByteData &data);

	// The input string is first decoded according to the encoding algorithm specified
	// by the EncodingMode property (such as base64, hex, etc.) It is then encrypted
	// according to the encryption algorithm specified by CryptAlgorithm. The resulting
	// encrypted data is encoded (using EncodingMode) and returned.
	bool EncryptEncoded(const char *str, CkString &outStr);
	// The input string is first decoded according to the encoding algorithm specified
	// by the EncodingMode property (such as base64, hex, etc.) It is then encrypted
	// according to the encryption algorithm specified by CryptAlgorithm. The resulting
	// encrypted data is encoded (using EncodingMode) and returned.
	const char *encryptEncoded(const char *str);

	// Encrypts a string and returns the encrypted data as a byte array. The minimal
	// set of properties that should be set before encrypting are: CryptAlgorithm,
	// SecretKey, Charset. Other properties that control encryption are: CipherMode,
	// PaddingScheme, KeyLength, IV. When decrypting, all property settings must match
	// otherwise garbled data is returned. The Charset property controls the exact
	// bytes that get encrypted. Languages such as VB.NET, C#, and Visual Basic work
	// with Unicode strings, thus the input string is Unicode. If Unicode is to be
	// encrypted (i.e. 2 bytes per character) then set the Charset property to
	// "Unicode". To implicitly convert the string to another charset before the
	// encryption is applied, set the Charset property to something else, such as
	// "iso-8859-1", "Shift_JIS", "big5", "windows-1252", etc. The complete list of
	// possible charsets is listed here:
	// us-ascii
	// unicode
	// unicodefffe
	// iso-8859-1
	// iso-8859-2
	// iso-8859-3
	// iso-8859-4
	// iso-8859-5
	// iso-8859-6
	// iso-8859-7
	// iso-8859-8
	// iso-8859-9
	// iso-8859-13
	// iso-8859-15
	// windows-874
	// windows-1250
	// windows-1251
	// windows-1252
	// windows-1253
	// windows-1254
	// windows-1255
	// windows-1256
	// windows-1257
	// windows-1258
	// utf-7
	// utf-8
	// utf-32
	// utf-32be
	// shift_jis
	// gb2312
	// ks_c_5601-1987
	// big5
	// iso-2022-jp
	// iso-2022-kr
	// euc-jp
	// euc-kr
	// macintosh
	// x-mac-japanese
	// x-mac-chinesetrad
	// x-mac-korean
	// x-mac-arabic
	// x-mac-hebrew
	// x-mac-greek
	// x-mac-cyrillic
	// x-mac-chinesesimp
	// x-mac-romanian
	// x-mac-ukrainian
	// x-mac-thai
	// x-mac-ce
	// x-mac-icelandic
	// x-mac-turkish
	// x-mac-croatian
	// asmo-708
	// dos-720
	// dos-862
	// ibm037
	// ibm437
	// ibm500
	// ibm737
	// ibm775
	// ibm850
	// ibm852
	// ibm855
	// ibm857
	// ibm00858
	// ibm860
	// ibm861
	// ibm863
	// ibm864
	// ibm865
	// cp866
	// ibm869
	// ibm870
	// cp875
	// koi8-r
	// koi8-u
	bool EncryptString(const char *str, CkByteData &outData);

	// Encrypts a string and returns the encrypted data as an encoded (printable)
	// string. The minimal set of properties that should be set before encrypting are:
	// CryptAlgorithm, SecretKey, Charset, and EncodingMode. Other properties that
	// control encryption are: CipherMode, PaddingScheme, KeyLength, IV. When
	// decrypting (with DecryptStringENC), all property settings must match otherwise
	// garbled data is returned. The Charset property controls the exact bytes that get
	// encrypted. Languages such as VB.NET, C#, and Visual Basic work with Unicode
	// strings, thus the input string is Unicode. If Unicode is to be encrypted (i.e. 2
	// bytes per character) then set the Charset property to "Unicode". To implicitly
	// convert the string to another charset before the encryption is applied, set the
	// Charset property to something else, such as "iso-8859-1", "Shift_JIS", "big5",
	// "windows-1252", etc. (Refer to EncryptString for the complete list of charsets.)
	// 
	// The EncodingMode property controls the encoding of the string that is returned.
	// It can be set to "Base64", "QP", or "Hex".
	// 
	bool EncryptStringENC(const char *str, CkString &outStr);
	// Encrypts a string and returns the encrypted data as an encoded (printable)
	// string. The minimal set of properties that should be set before encrypting are:
	// CryptAlgorithm, SecretKey, Charset, and EncodingMode. Other properties that
	// control encryption are: CipherMode, PaddingScheme, KeyLength, IV. When
	// decrypting (with DecryptStringENC), all property settings must match otherwise
	// garbled data is returned. The Charset property controls the exact bytes that get
	// encrypted. Languages such as VB.NET, C#, and Visual Basic work with Unicode
	// strings, thus the input string is Unicode. If Unicode is to be encrypted (i.e. 2
	// bytes per character) then set the Charset property to "Unicode". To implicitly
	// convert the string to another charset before the encryption is applied, set the
	// Charset property to something else, such as "iso-8859-1", "Shift_JIS", "big5",
	// "windows-1252", etc. (Refer to EncryptString for the complete list of charsets.)
	// 
	// The EncodingMode property controls the encoding of the string that is returned.
	// It can be set to "Base64", "QP", or "Hex".
	// 
	const char *encryptStringENC(const char *str);

	// Identical to the GenerateSecretKey method, except it returns the binary secret
	// key as a string encoded according to  encoding, which may be "base64", "hex", "url",
	// etc. Please see the documentation for GenerateSecretKey for more information.
	bool GenEncodedSecretKey(const char *password, const char *encoding, CkString &outStr);
	// Identical to the GenerateSecretKey method, except it returns the binary secret
	// key as a string encoded according to  encoding, which may be "base64", "hex", "url",
	// etc. Please see the documentation for GenerateSecretKey for more information.
	const char *genEncodedSecretKey(const char *password, const char *encoding);

	// Generates numBytes random bytes and returns them as an encoded string. The encoding,
	// such as base64, hex, etc. is controlled by the EncodingMode property.
	bool GenRandomBytesENC(int numBytes, CkString &outStr);
	// Generates numBytes random bytes and returns them as an encoded string. The encoding,
	// such as base64, hex, etc. is controlled by the EncodingMode property.
	const char *genRandomBytesENC(int numBytes);

	// Hashes a string to a byte array that has the same number of bits as the current
	// value of the KeyLength property. For example, if KeyLength is equal to 128 bits,
	// then a 16-byte array is returned. This can be used to set the SecretKey
	// property. In order to decrypt, the SecretKey must match exactly. To use
	// "password-based" encryption, the password is passed to this method to generate a
	// binary secret key that can then be assigned to the SecretKey property.
	// 
	// IMPORTANT: If you are trying to decrypt something encrypted by another party
	// such that the other party provided you with the secret key, DO NOT use this
	// method. This method is for transforming an arbitrary-length password into a
	// binary secret key of the proper length. Please see this Chilkat blog post:
	// Getting Started with AES Decryption
	// <http://www.cknotes.com/?p=290>
	// 
	bool GenerateSecretKey(const char *password, CkByteData &outData);

	// Returns the initialization vector as an encoded string. The encoding argument
	// can be set to any of the following strings: "base64", "hex", "quoted-printable",
	// or "url".
	bool GetEncodedIV(const char *encoding, CkString &outIV);
	// Returns the initialization vector as an encoded string. The encoding argument
	// can be set to any of the following strings: "base64", "hex", "quoted-printable",
	// or "url".
	const char *getEncodedIV(const char *encoding);
	// Returns the initialization vector as an encoded string. The encoding argument
	// can be set to any of the following strings: "base64", "hex", "quoted-printable",
	// or "url".
	const char *encodedIV(const char *encoding);

	// Returns the secret key as an encoded string. The encoding argument can be set to
	// any of the following strings: "base64", "hex", "quoted-printable", or "url".
	bool GetEncodedKey(const char *encoding, CkString &outKey);
	// Returns the secret key as an encoded string. The encoding argument can be set to
	// any of the following strings: "base64", "hex", "quoted-printable", or "url".
	const char *getEncodedKey(const char *encoding);
	// Returns the secret key as an encoded string. The encoding argument can be set to
	// any of the following strings: "base64", "hex", "quoted-printable", or "url".
	const char *encodedKey(const char *encoding);

	// Returns the password-based encryption (PBE) salt bytes as an encoded string. The
	// encoding argument can be set to any of the following strings: "base64", "hex",
	// "quoted-printable", or "url".
	bool GetEncodedSalt(const char *encoding, CkString &outStr);
	// Returns the password-based encryption (PBE) salt bytes as an encoded string. The
	// encoding argument can be set to any of the following strings: "base64", "hex",
	// "quoted-printable", or "url".
	const char *getEncodedSalt(const char *encoding);
	// Returns the password-based encryption (PBE) salt bytes as an encoded string. The
	// encoding argument can be set to any of the following strings: "base64", "hex",
	// "quoted-printable", or "url".
	const char *encodedSalt(const char *encoding);

	// Returns the last certificate used when verifying a signature. This method is
	// deprecated. Applications should instead call GetSignerCert with an index of 0.
	// The caller is responsible for deleting the object returned by this method.
	CkCert *GetLastCert(void);

	// This method can be called after a digital signature is verified to retrieve the
	// signing time of the Nth certificate used for signing. The 1st
	// certificate/signing time is at index 0. The NumSignerCerts property contains the
	// total number of signing certificates. (Typically, a single certificate is used
	// in creating a digital signature.)
	// 
	// Note: An application should first check to see if a signing date/time is
	// available for the Nth certificate by calling the HasSignatureSigningTime method.
	// The indices for which there is no signing time available should be skipped.
	// 
	bool GetSignatureSigningTime(int index, SYSTEMTIME &outSysTime);

	// The same as GetSignatureSigningTime, except the date/time is returned in RFC822
	// string format.
	bool GetSignatureSigningTimeStr(int index, CkString &outStr);
	// The same as GetSignatureSigningTime, except the date/time is returned in RFC822
	// string format.
	const char *getSignatureSigningTimeStr(int index);
	// The same as GetSignatureSigningTime, except the date/time is returned in RFC822
	// string format.
	const char *signatureSigningTimeStr(int index);

	// Gets the Nth certificate used for signing. This method can be called after
	// verifying a digital signature to get the signer certs. The 1st certificate is at
	// index 0. The NumSignerCerts property contains the total number of signing
	// certificates. (Typically, a single certificate is used in creating a digital
	// signature.)
	// The caller is responsible for deleting the object returned by this method.
	CkCert *GetSignerCert(int index);

	// This method can be called after a digital signature has been verified by one of
	// the Verify* methods. Returns true if a signing time for the Nth certificate is
	// available and can be retrieved by either the GetSignatureSigningTime or
	// GetSignatureSigningTimeStr methods.
	bool HasSignatureSigningTime(int index);

	// Begin hashing a byte stream. Call this method to hash the 1st chunk. Additional
	// chunks are hashed by calling HashMoreBytes 0 or more times followed by a final
	// call to HashFinal (or HashFinalENC) to retrieve the result. The hash algorithm
	// is selected by the HashAlgorithm property setting.
	bool HashBeginBytes(const CkByteData &data);

	// Begin hashing a text stream. Call this method to hash the 1st chunk. Additional
	// chunks are hashed by calling HashMoreString 0 or more times followed by a final
	// call to HashFinal (or HashFinalENC) to retrieve the result. The hash algorithm
	// is selected by the HashAlgorithm property setting.
	bool HashBeginString(const char *strData);

	// Hashes a byte array. The hash algorithm to be used is controlled by the
	// HashAlgorithm property, which can be set to "sha1", "sha384", "sha512", "md2",
	// "md5", "haval", "ripemd128", "ripemd160","ripemd256", or "ripemd320".
	// 
	// The HAVAL hash algorithm is affected by two other properties: HavalRounds and
	// KeyLength. The HavalRounds may have values of 3, 4, or 5. The KeyLength may have
	// values of 128, 160, 192, 224, or 256.
	// 
	bool HashBytes(const CkByteData &data, CkByteData &outData);

	// Hashes a byte array and returns an encoded (printable) string of the binary
	// hash. The hash algorithm to be used is controlled by the HashAlgorithm property,
	// which can be set to "sha1", "sha384", "sha512", "md2", "md5", or "haval". The
	// encoding is controlled by the EncodingMode property, which can be set to
	// "Base64", "QP", or "Hex".
	bool HashBytesENC(const CkByteData &data, CkString &outStr);
	// Hashes a byte array and returns an encoded (printable) string of the binary
	// hash. The hash algorithm to be used is controlled by the HashAlgorithm property,
	// which can be set to "sha1", "sha384", "sha512", "md2", "md5", or "haval". The
	// encoding is controlled by the EncodingMode property, which can be set to
	// "Base64", "QP", or "Hex".
	const char *hashBytesENC(const CkByteData &data);

	// Hashes a file. The hash algorithm to be used is controlled by the HashAlgorithm
	// property, which can be set to "sha1", "sha384", "sha512", "md2", "md5", "haval",
	// "ripemd128", "ripemd160","ripemd256", or "ripemd320". The function returns the
	// hash bytes. Any size file is supported because the file is hashed internally in
	// streaming mode.
	// 
	// The HAVAL hash algorithm is affected by two other properties: HavalRounds and
	// KeyLength. The HavalRounds may have values of 3, 4, or 5. The KeyLength may have
	// values of 128, 160, 192, 224, or 256.
	// 
	bool HashFile(const char *filename, CkByteData &outBytes);

	// Hashes a file. The hash algorithm to be used is controlled by the HashAlgorithm
	// property, which can be set to "sha1", "sha384", "sha512", "md2", "md5", or
	// "haval". The function returns the hash as a string encoded according to the
	// EncodingMode property. Any size file is supported because the file is hashed
	// internally in streaming mode.
	bool HashFileENC(const char *filename, CkString &outStr);
	// Hashes a file. The hash algorithm to be used is controlled by the HashAlgorithm
	// property, which can be set to "sha1", "sha384", "sha512", "md2", "md5", or
	// "haval". The function returns the hash as a string encoded according to the
	// EncodingMode property. Any size file is supported because the file is hashed
	// internally in streaming mode.
	const char *hashFileENC(const char *filename);

	// Finalizes a multi-step hash computation and returns the hash bytes.
	bool HashFinal(CkByteData &outBytes);

	// Finalizes a multi-step hash computation and returns the hash bytes encoded
	// according to the EncodingMode property setting.
	bool HashFinalENC(CkString &outStr);
	// Finalizes a multi-step hash computation and returns the hash bytes encoded
	// according to the EncodingMode property setting.
	const char *hashFinalENC(void);

	// Adds more bytes to the hash currently under computation. (See HashBeginBytes)
	bool HashMoreBytes(const CkByteData &data);

	// Adds more text to the hash currently under computation. (See HashBeginString)
	bool HashMoreString(const char *strData);

	// Hashes a string and returns a binary hash. The hash algorithm to be used is
	// controlled by the HashAlgorithm property, which can be set to "sha1", "sha384",
	// "sha512", "md2", "md5", "haval", "ripemd128", "ripemd160","ripemd256", or
	// "ripemd320".
	// 
	// The Charset property controls the character encoding of the string that is
	// hashed. Languages such as VB.NET, C#, and Visual Basic work with Unicode
	// strings. If it is desired to hash Unicode directly (2 bytes/char) then set the
	// Charset property to "Unicode". To implicitly convert to another charset before
	// hashing, set the Charset property to the desired charset. For example, if
	// Charset is set to "iso-8859-1", the input string is first implicitly converted
	// to iso-8859-1 (1 byte per character) before hashing. The full list fo supported
	// charsets is listed in the EncryptString method description.
	// 
	bool HashString(const char *str, CkByteData &outData);

	// Hashes a string and returns an encoded (printable) string of the binary hash.
	// The hash algorithm to be used is controlled by the HashAlgorithm property, which
	// can be set to "sha1", "sha384", "sha512", "md2", "md5", "haval", "ripemd128",
	// "ripemd160","ripemd256", or "ripemd320".
	// 
	// The Charset property controls the character encoding of the string that is
	// hashed. Languages such as VB.NET, C#, and Visual Basic work with Unicode
	// strings. If it is desired to hash Unicode directly (2 bytes/char) then set the
	// Charset property to "Unicode". To implicitly convert to another charset before
	// hashing, set the Charset property to the desired charset. For example, if
	// Charset is set to "iso-8859-1", the input string is first implicitly converted
	// to iso-8859-1 (1 byte per character) before hashing. The full list of supported
	// charsets is listed in the EncryptString method description.
	// The encoding of the output string is controlled by the EncodingMode property,
	// which can be set to "Base64", "QP", or "Hex".
	// 
	// The HAVAL hash algorithm is affected by two other properties: HavalRounds and
	// KeyLength. The HavalRounds may have values of 3, 4, or 5. The KeyLength may have
	// values of 128, 160, 192, 224, or 256.
	// 
	bool HashStringENC(const char *str, CkString &outStr);
	// Hashes a string and returns an encoded (printable) string of the binary hash.
	// The hash algorithm to be used is controlled by the HashAlgorithm property, which
	// can be set to "sha1", "sha384", "sha512", "md2", "md5", "haval", "ripemd128",
	// "ripemd160","ripemd256", or "ripemd320".
	// 
	// The Charset property controls the character encoding of the string that is
	// hashed. Languages such as VB.NET, C#, and Visual Basic work with Unicode
	// strings. If it is desired to hash Unicode directly (2 bytes/char) then set the
	// Charset property to "Unicode". To implicitly convert to another charset before
	// hashing, set the Charset property to the desired charset. For example, if
	// Charset is set to "iso-8859-1", the input string is first implicitly converted
	// to iso-8859-1 (1 byte per character) before hashing. The full list of supported
	// charsets is listed in the EncryptString method description.
	// The encoding of the output string is controlled by the EncodingMode property,
	// which can be set to "Base64", "QP", or "Hex".
	// 
	// The HAVAL hash algorithm is affected by two other properties: HavalRounds and
	// KeyLength. The HavalRounds may have values of 3, 4, or 5. The KeyLength may have
	// values of 128, 160, 192, 224, or 256.
	// 
	const char *hashStringENC(const char *str);

	// Computes a keyed-Hash Message Authentication Code (HMAC or KHMAC), which is a
	// type of message authentication code (MAC) calculated using a specific algorithm
	// involving a cryptographic hash function in combination with a secret key. As
	// with any MAC, it may be used to simultaneously verify both the data integrity
	// and the authenticity of a message. Any iterative cryptographic hash function,
	// such as MD5 or SHA-1, may be used in the calculation of an HMAC; the resulting
	// MAC algorithm is termed HMAC-MD5 or HMAC-SHA-1 accordingly. The cryptographic
	// strength of the HMAC depends upon the cryptographic strength of the underlying
	// hash function, on the size and quality of the key and the size of the hash
	// output length in bits.
	// 
	// The secret key is set by calling one of these methods prior to calling this
	// method: SetHmacKeyBytes, SetHmacKeyEncoded, or SetHmacKeyString.
	// 
	// The hash algorithm is specified by the HashAlgorithm property.
	// 
	bool HmacBytes(const CkByteData &inBytes, CkByteData &outHmac);

	// Computes an HMAC using a secret key and hash algorithm. The result is encoded to
	// a string using the encoding (base64, hex, etc.) specified by the EncodingMode
	// property.
	// 
	// The secret key is set by calling one of these methods prior to calling this
	// method: SetHmacKeyBytes, SetHmacKeyEncoded, or SetHmacKeyString.
	// 
	// The hash algorithm is specified by the HashAlgorithm property.
	// 
	bool HmacBytesENC(const CkByteData &inBytes, CkString &outEncodedHmac);
	// Computes an HMAC using a secret key and hash algorithm. The result is encoded to
	// a string using the encoding (base64, hex, etc.) specified by the EncodingMode
	// property.
	// 
	// The secret key is set by calling one of these methods prior to calling this
	// method: SetHmacKeyBytes, SetHmacKeyEncoded, or SetHmacKeyString.
	// 
	// The hash algorithm is specified by the HashAlgorithm property.
	// 
	const char *hmacBytesENC(const CkByteData &inBytes);

	// Computes an HMAC using a secret key and hash algorithm.
	// 
	// The secret key is set by calling one of these methods prior to calling this
	// method: SetHmacKeyBytes, SetHmacKeyEncoded, or SetHmacKeyString.
	// 
	// The hash algorithm is specified by the HashAlgorithm property.
	// 
	bool HmacString(const char *inText, CkByteData &outHmac);

	// Computes an HMAC using a secret key and hash algorithm. The result is encoded to
	// a string using the encoding (base64, hex, etc.) specified by the EncodingMode
	// property.
	// 
	// The secret key is set by calling one of these methods prior to calling this
	// method: SetHmacKeyBytes, SetHmacKeyEncoded, or SetHmacKeyString.
	// 
	// The hash algorithm is specified by the HashAlgorithm property.
	// 
	bool HmacStringENC(const char *inText, CkString &outEncodedHmac);
	// Computes an HMAC using a secret key and hash algorithm. The result is encoded to
	// a string using the encoding (base64, hex, etc.) specified by the EncodingMode
	// property.
	// 
	// The secret key is set by calling one of these methods prior to calling this
	// method: SetHmacKeyBytes, SetHmacKeyEncoded, or SetHmacKeyString.
	// 
	// The hash algorithm is specified by the HashAlgorithm property.
	// 
	const char *hmacStringENC(const char *inText);

	// The opposite of CompressBytes.
	bool InflateBytes(const CkByteData &data, CkByteData &outData);

	// The opposite of CompressBytesENC. The EncodingMode and CompressionAlgorithm
	// properties should match what was used when compressing.
	bool InflateBytesENC(const char *str, CkByteData &outData);

	// The opposite of CompressString. The Charset and CompressionAlgorithm properties
	// should match what was used when compressing.
	bool InflateString(const CkByteData &data, CkString &outStr);
	// The opposite of CompressString. The Charset and CompressionAlgorithm properties
	// should match what was used when compressing.
	const char *inflateString(const CkByteData &data);

	// The opposite of CompressStringENC. The Charset, EncodingMode, and
	// CompressionAlgorithm properties should match what was used when compressing.
	bool InflateStringENC(const char *str, CkString &outStr);
	// The opposite of CompressStringENC. The Charset, EncodingMode, and
	// CompressionAlgorithm properties should match what was used when compressing.
	const char *inflateStringENC(const char *str);

	// Returns true if the component is unlocked.
	bool IsUnlocked(void);

	// Matches MySQL's AES_DECRYPT function. strEncryptedHex is a hex-encoded string of the AES
	// encrypted data. The return value is the original unencrypted string.
	bool MySqlAesDecrypt(const char *strEncrypted, const char *strKey, CkString &outStr);
	// Matches MySQL's AES_DECRYPT function. strEncryptedHex is a hex-encoded string of the AES
	// encrypted data. The return value is the original unencrypted string.
	const char *mySqlAesDecrypt(const char *strEncrypted, const char *strKey);

	// Matches MySQL's AES_ENCRYPT function. The return value is a hex-encoded string
	// of the encrypted data. The equivalent call in MySQL would look like this:
	// HEX(AES_ENCRYPT('The quick brown fox jumps over the lazy dog','password'))
	bool MySqlAesEncrypt(const char *strData, const char *strKey, CkString &outStr);
	// Matches MySQL's AES_ENCRYPT function. The return value is a hex-encoded string
	// of the encrypted data. The equivalent call in MySQL would look like this:
	// HEX(AES_ENCRYPT('The quick brown fox jumps over the lazy dog','password'))
	const char *mySqlAesEncrypt(const char *strData, const char *strKey);

	// Digitally signs a byte array and returns a PKCS7/CMS format signature. This is a
	// signature that contains both the original data as well as the signature. A
	// certificate must be set by calling SetSigningCert prior to calling this method.
	bool OpaqueSignBytes(const CkByteData &data, CkByteData &outData);

	// Digitally signs a byte array and returns a PKCS7/CMS format signature in encoded
	// string format (such as Base64 or hex). This is a signature that contains both
	// the original data as well as the signature. A certificate must be set by calling
	// SetSigningCert prior to calling this method. The EncodingMode property controls
	// the output encoding, which can be "Base64", "QP","Hex", etc. (See the
	// EncodingMode property.)
	bool OpaqueSignBytesENC(const CkByteData &data, CkString &outStr);
	// Digitally signs a byte array and returns a PKCS7/CMS format signature in encoded
	// string format (such as Base64 or hex). This is a signature that contains both
	// the original data as well as the signature. A certificate must be set by calling
	// SetSigningCert prior to calling this method. The EncodingMode property controls
	// the output encoding, which can be "Base64", "QP","Hex", etc. (See the
	// EncodingMode property.)
	const char *opaqueSignBytesENC(const CkByteData &data);

	// Digitally signs a string and returns a PKCS7/CMS format signature. This is a
	// signature that contains both the original data as well as the signature. A
	// certificate must be set by calling SetSigningCert prior to calling this method.
	// The Charset property controls the character encoding of the string that is
	// signed. (Languages such as VB.NET, C#, and Visual Basic work with Unicode
	// strings.) To sign Unicode data (2 bytes per char), set the Charset property to
	// "Unicode". To implicitly convert the string to a mutlibyte charset such as
	// "iso-8859-1", "Shift_JIS", "utf-8", or something else, then set the Charset
	// property to the name of the charset before signing. The complete list of
	// charsets is listed in the EncryptString method description.
	bool OpaqueSignString(const char *str, CkByteData &outData);

	// Digitally signs a string and returns a PKCS7/CMS format signature in encoded
	// string format (such as base64 or hex). This is a signature that contains both
	// the original data as well as the signature. A certificate must be set by calling
	// SetSigningCert prior to calling this method. The Charset property controls the
	// character encoding of the string that is signed. (Languages such as VB.NET, C#,
	// and Visual Basic work with Unicode strings.) To sign Unicode data (2 bytes per
	// char), set the Charset property to "Unicode". To implicitly convert the string
	// to a mutlibyte charset such as "iso-8859-1", "Shift_JIS", "utf-8", or something
	// else, then set the Charset property to the name of the charset before signing.
	// The complete list of charsets is listed in the EncryptString method description.
	// 
	// The EncodingMode property controls the output encoding, which can be "Base64",
	// "QP","Hex", etc. (See the EncodingMode property.)
	// 
	bool OpaqueSignStringENC(const char *str, CkString &outStr);
	// Digitally signs a string and returns a PKCS7/CMS format signature in encoded
	// string format (such as base64 or hex). This is a signature that contains both
	// the original data as well as the signature. A certificate must be set by calling
	// SetSigningCert prior to calling this method. The Charset property controls the
	// character encoding of the string that is signed. (Languages such as VB.NET, C#,
	// and Visual Basic work with Unicode strings.) To sign Unicode data (2 bytes per
	// char), set the Charset property to "Unicode". To implicitly convert the string
	// to a mutlibyte charset such as "iso-8859-1", "Shift_JIS", "utf-8", or something
	// else, then set the Charset property to the name of the charset before signing.
	// The complete list of charsets is listed in the EncryptString method description.
	// 
	// The EncodingMode property controls the output encoding, which can be "Base64",
	// "QP","Hex", etc. (See the EncodingMode property.)
	// 
	const char *opaqueSignStringENC(const char *str);

	// Verifies an opaque signature and returns the original data. If the signature
	// verification fails, the returned data will be 0 bytes in length.
	bool OpaqueVerifyBytes(const CkByteData &p7s, CkByteData &outOriginal);

	// Verifies an opaque signature (encoded in string form) and returns the original
	// data. If the signature verification fails, the returned data will be 0 bytes in
	// length.
	bool OpaqueVerifyBytesENC(const char *p7s, CkByteData &outOriginal);

	// Verifies an opaque signature and returns the original string. If the signature
	// verification fails, the returned string will be 0 characters in length.
	bool OpaqueVerifyString(const CkByteData &p7s, CkString &outOriginal);
	// Verifies an opaque signature and returns the original string. If the signature
	// verification fails, the returned string will be 0 characters in length.
	const char *opaqueVerifyString(const CkByteData &p7s);

	// Verifies an opaque signature (encoded in string form) and returns the original
	// data string. If the signature verification fails, the returned string will be 0
	// characters in length.
	bool OpaqueVerifyStringENC(const char *p7s, CkString &outOriginal);
	// Verifies an opaque signature (encoded in string form) and returns the original
	// data string. If the signature verification fails, the returned string will be 0
	// characters in length.
	const char *opaqueVerifyStringENC(const char *p7s);

	// Implements the PBKDF1 algorithm (Password Based Key Derivation Function #1). The
	// password is converted to the character encoding represented by  charset before being
	// passed (internally) to the key derivation function. The  hashAlg may be "md5",
	// "sha1", "md2", etc. The  salt should be random data at least 8 bytes (64 bits) in
	// length. (The GenRandomBytesENC method is good for generating a random salt
	// value.) The  iterationCount should be no less than 1000. The length (in bits) of the
	// derived key output by this method is controlled by  outputKeyBitLen. The  encoding argument may
	// be "base64", "hex", etc. It controls the encoding of the output, and the
	// expected encoding of the  salt. The derived key is returned.
	bool Pbkdf1(const char *password, const char *charset, const char *hashAlg, const char *salt, int iterationCount, int outputKeyBitLen, const char *encoding, CkString &outStr);
	// Implements the PBKDF1 algorithm (Password Based Key Derivation Function #1). The
	// password is converted to the character encoding represented by  charset before being
	// passed (internally) to the key derivation function. The  hashAlg may be "md5",
	// "sha1", "md2", etc. The  salt should be random data at least 8 bytes (64 bits) in
	// length. (The GenRandomBytesENC method is good for generating a random salt
	// value.) The  iterationCount should be no less than 1000. The length (in bits) of the
	// derived key output by this method is controlled by  outputKeyBitLen. The  encoding argument may
	// be "base64", "hex", etc. It controls the encoding of the output, and the
	// expected encoding of the  salt. The derived key is returned.
	const char *pbkdf1(const char *password, const char *charset, const char *hashAlg, const char *salt, int iterationCount, int outputKeyBitLen, const char *encoding);

	// Implements the PBKDF2 algorithm (Password Based Key Derivation Function #2). The
	// password is converted to the character encoding represented by  charset before being
	// passed (internally) to the key derivation function. The  hashAlg may be "md5",
	// "sha1", "md2", etc. The  salt should be random data at least 8 bytes (64 bits) in
	// length. (The GenRandomBytesENC method is good for generating a random salt
	// value.) The  iterationCount should be no less than 1000. The length (in bits) of the
	// derived key output by this method is controlled by  outputKeyBitLen. The  encoding argument may
	// be "base64", "hex", etc. It controls the encoding of the output, and the
	// expected encoding of the  salt. The derived key is returned.
	bool Pbkdf2(const char *password, const char *charset, const char *hashAlg, const char *salt, int iterationCount, int outputKeyBitLen, const char *encoding, CkString &outStr);
	// Implements the PBKDF2 algorithm (Password Based Key Derivation Function #2). The
	// password is converted to the character encoding represented by  charset before being
	// passed (internally) to the key derivation function. The  hashAlg may be "md5",
	// "sha1", "md2", etc. The  salt should be random data at least 8 bytes (64 bits) in
	// length. (The GenRandomBytesENC method is good for generating a random salt
	// value.) The  iterationCount should be no less than 1000. The length (in bits) of the
	// derived key output by this method is controlled by  outputKeyBitLen. The  encoding argument may
	// be "base64", "hex", etc. It controls the encoding of the output, and the
	// expected encoding of the  salt. The derived key is returned.
	const char *pbkdf2(const char *password, const char *charset, const char *hashAlg, const char *salt, int iterationCount, int outputKeyBitLen, const char *encoding);

	// Sets the initialization vector to a random value.
	void RandomizeIV(void);

	// Sets the secret key to a random value.
	void RandomizeKey(void);

	// Provides a means for converting from one encoding to another (such as base64 to
	// hex). This is helpful for programming environments where byte arrays are a real
	// pain-in-the-***. The  fromEncoding and  toEncoding may be "base64", "hex", "quoted-printable"
	// (or "qp"), or "url".
	bool ReEncode(const char *data, const char *fromEncoding, const char *toEncoding, CkString &outStr);
	// Provides a means for converting from one encoding to another (such as base64 to
	// hex). This is helpful for programming environments where byte arrays are a real
	// pain-in-the-***. The  fromEncoding and  toEncoding may be "base64", "hex", "quoted-printable"
	// (or "qp"), or "url".
	const char *reEncode(const char *data, const char *fromEncoding, const char *toEncoding);

	// Convenience method to read an entire file and return as a byte array.
	bool ReadFile(const char *filename, CkByteData &outBytes);

#if defined(CK_CSP_INCLUDED)
	// (Only applies to the Microsoft Windows OS) Sets the Cryptographic Service
	// Provider (CSP) to be used for PKI public-key encryption / signing, or public-key
	// decryption / signature verification.
	// 
	// This is not commonly used becaues the default Microsoft CSP is typically
	// appropriate. One instance where SetCSP is necessary is when using the Crypto-Pro
	// CSP for the GOST R 34.10-2001 and GOST R 34.10-94 providers.
	// 
	bool SetCSP(const CkCsp &csp);
#endif

	// Sets the digital certificate to be used for decryption when the CryptAlgorithm
	// property is set to "PKI". A private key is required for decryption. Because this
	// method only specifies the certificate, a prerequisite is that the certificate w/
	// private key must have been pre-installed on the computer. Private keys are
	// stored in the Windows Protected Store (either a user account specific store, or
	// the system-wide store). The Chilkat component will automatically locate and find
	// the certificate's corresponding private key from the protected store when
	// decrypting.
	bool SetDecryptCert(CkCert &cert);

	// Sets the digital certificate to be used for decryption when the CryptAlgorithm
	// property is set to "PKI". The private key is supplied in the 2nd argument to
	// this method, so there is no requirement that the certificate be pre-installed on
	// a computer before decrypting (if this method is called).
	bool SetDecryptCert2(const CkCert &cert, CkPrivateKey &key);

	// Sets the initialization vector from an encoded string. The encoding argument can
	// be set to any of the following strings: "base64", "hex", "quoted-printable",
	// "ascii", or "url".
	void SetEncodedIV(const char *ivStr, const char *encoding);

	// Sets the secret key from an encoded string. The encoding argument can be set to
	// any of the following strings: "base64", "hex", "quoted-printable", "ascii", or
	// "url".
	void SetEncodedKey(const char *keyStr, const char *encoding);

	// Sets the password-based encryption (PBE) salt bytes from an encoded string. The
	// encoding argument can be set to any of the following strings: "base64", "hex",
	// "quoted-printable", "ascii", or "url".
	void SetEncodedSalt(const char *saltStr, const char *encoding);

	// Tells the encryption library to use a specific digital certificate for
	// public-key encryption. To encrypt with multiple certificates, call
	// AddEncryptCert once for each certificate. (Calling this method is the equivalent
	// of calling ClearEncryptCerts followed by AddEncryptCert.)
	bool SetEncryptCert(const CkCert &cert);

	// Sets the secret key to be used for one of the HMAC methods.
	void SetHmacKeyBytes(const CkByteData &keyBytes);

	// Sets the secret key to be used for one of the HMAC methods. The  encoding can be set
	// to any of the following strings: "base64", "hex", "quoted-printable", or "url".
	void SetHmacKeyEncoded(const char *key, const char *encoding);

	// Sets the secret key to be used for one of the HMAC methods.
	void SetHmacKeyString(const char *key);

	// Sets the initialization vector for a symmetric encryption algorithm (such as
	// AES, BlowFish, TwoFish, DES, etc.). IV's are used in CBC mode
	// (Cipher-Block-Chaining), but are not used in ECB mode (Electronic Cookbook). The
	// length of the IV should equal the block size of the algorithm. (It is not equal
	// to the key length). For AES and TwoFish, the block size (and thus IV size) is
	// always 16 bytes. For Blowfish it's 8 bytes. For DES and 3DES it's 8 bytes.
	void SetIV(const unsigned char *pByteData, unsigned long szByteData);

	// Sets the value of the SecretKey property.
	void SetSecretKey(const unsigned char *pByteData, unsigned long szByteData);

	// Accepts a password string and (internally) generates a binary secret key of the
	// appropriate bit length and sets the SecretKey property. This method should only
	// be used if you are using Chilkat for both encryption and decryption because the
	// password-to-secret-key algorithm would need to be identical for the decryption
	// to match the encryption.
	void SetSecretKeyViaPassword(const char *password);

	// Specifies a certificate to be used when creating PKCS7 digital signatures.
	// Signing requires both a certificate and private key. In this case, the private
	// key is implicitly specified if the certificate originated from a PFX that
	// contains the corresponding private key, or if on a Windows-based computer where
	// the certificate and corresponding private key are pre-installed. (If a PFX file
	// is used, it is provided via the AddPfxSourceFile or AddPfxSourceData methods.)
	bool SetSigningCert(const CkCert &cert);

	// Specifies a digital certificate and private key to be used for creating PKCS7
	// digital signatures.
	bool SetSigningCert2(const CkCert &cert, CkPrivateKey &key);

	// Sets the digital certificate to be used in verifying a signature.
	bool SetVerifyCert(const CkCert &cert);

	// Digitally signs a byte array and returns the detached digital signature. A
	// certificate must be set by calling SetSigningCert prior to calling this method.
	bool SignBytes(const CkByteData &data, CkByteData &outData);

	// Digitally signs a byte array and returns the detached digital signature encoded
	// as a printable string. A certificate must be set by calling SetSigningCert prior
	// to calling this method. The EncodingMode property controls the output encoding,
	// which can be "Base64", "QP", or "Hex".
	bool SignBytesENC(const CkByteData &data, CkString &outStr);
	// Digitally signs a byte array and returns the detached digital signature encoded
	// as a printable string. A certificate must be set by calling SetSigningCert prior
	// to calling this method. The EncodingMode property controls the output encoding,
	// which can be "Base64", "QP", or "Hex".
	const char *signBytesENC(const CkByteData &data);

	// Digitally signs a string and returns the detached digital signature. A
	// certificate must be set by calling SetSigningCert prior to calling this method.
	// The Charset property controls the character encoding of the string that is
	// signed. (Languages such as VB.NET, C#, and Visual Basic work with Unicode
	// strings.) To sign Unicode data (2 bytes per char), set the Charset property to
	// "Unicode". To implicitly convert the string to a mutlibyte charset such as
	// "iso-8859-1", "Shift_JIS", "utf-8", or something else, then set the Charset
	// property to the name of the charset before signing. The complete list of
	// charsets is listed in the EncryptString method description.
	bool SignString(const char *str, CkByteData &outData);

	// Digitally signs a string and returns the PKCS7 detached digital signature as an
	// encoded string. A certificate must be set by calling SetSigningCert prior to
	// calling this method. The Charset property controls the character encoding of the
	// string that is signed. (Languages such as VB.NET, C#, and Visual Basic work with
	// Unicode strings.) To sign Unicode data (2 bytes per char), set the Charset
	// property to "Unicode". To implicitly convert the string to a mutlibyte charset
	// such as "iso-8859-1", "Shift_JIS", "utf-8", or something else, then set the
	// Charset property to the name of the charset before signing. The complete list of
	// charsets is listed in the EncryptString method description.
	// 
	// The encoding of the output string is controlled by the EncodingMode property,
	// which can be set to "Base64", "QP", or "Hex".
	// 
	bool SignStringENC(const char *str, CkString &outStr);
	// Digitally signs a string and returns the PKCS7 detached digital signature as an
	// encoded string. A certificate must be set by calling SetSigningCert prior to
	// calling this method. The Charset property controls the character encoding of the
	// string that is signed. (Languages such as VB.NET, C#, and Visual Basic work with
	// Unicode strings.) To sign Unicode data (2 bytes per char), set the Charset
	// property to "Unicode". To implicitly convert the string to a mutlibyte charset
	// such as "iso-8859-1", "Shift_JIS", "utf-8", or something else, then set the
	// Charset property to the name of the charset before signing. The complete list of
	// charsets is listed in the EncryptString method description.
	// 
	// The encoding of the output string is controlled by the EncodingMode property,
	// which can be set to "Base64", "QP", or "Hex".
	// 
	const char *signStringENC(const char *str);

	// Convert a string to a byte array where the characters are encoded according to
	// the charset specified.
	bool StringToBytes(const char *inStr, const char *charset, CkByteData &outBytes);

	// Trim a string ending with a specific substring until the string no longer ends
	// with that substring.
	bool TrimEndingWith(const char *inStr, const char *ending, CkString &outStr);
	// Trim a string ending with a specific substring until the string no longer ends
	// with that substring.
	const char *trimEndingWith(const char *inStr, const char *ending);

	// Unlocks the component. This must be called once prior to calling any other
	// method.
	bool UnlockComponent(const char *b1);

	// Verifies a byte array against a digital signature and returns true if the byte
	// array is unaltered.
	bool VerifyBytes(const CkByteData &data, const CkByteData &sig);

	// Verifies a byte array against a string-encoded digital signature and returns
	// true if the byte array is unaltered. This method can be used to verify a
	// signature produced by SignBytesENC. The EncodingMode property must be set prior
	// to calling to match the encoding of the digital signature string ("Base64",
	// "QP", or "Hex").
	bool VerifyBytesENC(const CkByteData &data, const char *encodedSig);

	// Verifies a .p7s (PKCS #7 Signature) against the original file (or exact copy of
	// it). If the inFilename has not been modified, the return value is true, otherwise it
	// is false. This method is equivalent to VerifyP7S.
	bool VerifyDetachedSignature(const char *filename, const char *sigFile);

	// Verifies a .p7m file and extracts the original file from the .p7m. Returns
	// true if the signature is valid and the contents are unchanged. Otherwise
	// returns false.
	bool VerifyP7M(const char *p7mPath, const char *destPath);

	// Verifies a .p7s (PKCS #7 Signature) against the original file (or exact copy of
	// it). If the inFilename has not been modified, the return value is true, otherwise it
	// is false.
	bool VerifyP7S(const char *inFilename, const char *p7sFilename);

	// Verifies a string against a binary digital signature and returns true if the
	// string is unaltered. This method can be used to verify a signature produced by
	// SignString. The Charset property must be set to the charset that was used when
	// creating the signature.
	bool VerifyString(const char *str, const CkByteData &sig);

	// Verifies a string against a string-encoded digital signature and returns true if
	// the string is unaltered. This method can be used to verify a signature produced
	// by SignStringENC. The Charset and EncodingMode properties must be set to the
	// same values that were used when creating the signature.
	bool VerifyStringENC(const char *str, const char *encodedSig);

	// Convenience method to write an entire byte array to a file.
	bool WriteFile(const char *filename, const CkByteData &fileData);

	// Calculates a CRC for the contents of a file. To compute the CRC used in the Zip
	// file format, pass "CRC-32" for the ARG1. (The ARG1 argument provides the
	// flexibility to add additional CRC algorithms on an as-needed basis in the
	// future.) A value of 0 is returned if the file is unable to be read. Given that
	// there is a 1 in 4 billion chance of having an actual CRC of 0, an application
	// might choose to react to a 0 return value by testing to see if the file can be
	// opened and read.
	unsigned long CrcFile(const char *crcAlg, const char *path);

	// Calculates a CRC for in-memory byte data. To compute the CRC used in the Zip
	// file format, pass "CRC-32" for the ARG1. (The ARG1 argument provides the
	// flexibility to add additional CRC algorithms on an as-needed basis in the
	// future.)
	unsigned long CrcBytes(const char *crcAlg, const CkByteData &byteData);

	// Adds an XML certificate vault to the object's internal list of sources to be
	// searched for certificates and private keys when encrypting/decrypting or
	// signing/verifying. Unlike the AddPfxSourceData and AddPfxSourceFile methods,
	// only a single XML certificate vault can be used. If UseCertVault is called
	// multiple times, only the last certificate vault will be used, as each call to
	// UseCertVault will replace the certificate vault provided in previous calls.
	bool UseCertVault(CkXmlCertVault &vault);

	// Returns the full certificate chain for the Nth certificate used to sign the MIME
	// message. Indexing begins at 0.
	// The caller is responsible for deleting the object returned by this method.
	CkCertChain *GetSignerCertChain(int index);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
