// CkSshKey.h: interface for the CkSshKey class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkSshKey_H
#define _CkSshKey_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"




#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkSshKey
class CK_VISIBLE_PUBLIC CkSshKey  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkSshKey(const CkSshKey &);
	CkSshKey &operator=(const CkSshKey &);

    public:
	CkSshKey(void);
	virtual ~CkSshKey(void);

	static CkSshKey *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// true if the object contains a DSA key. false if the object contains an RSA
	// key.
	bool get_IsDsaKey(void);

	// true if the object contains a private key. false if it contains a public
	// key.
	bool get_IsPrivateKey(void);

	// true if the object contains an RSA key. false if the object contains a DSA
	// key.
	bool get_IsRsaKey(void);

	// The password to be used when importing or exporting encrypted private keys.
	void get_Password(CkString &str);
	// The password to be used when importing or exporting encrypted private keys.
	const char *password(void);
	// The password to be used when importing or exporting encrypted private keys.
	void put_Password(const char *newVal);

	// Some key file formats allow a comment to be included. This is the comment if
	// present.
	void get_Comment(CkString &str);
	// Some key file formats allow a comment to be included. This is the comment if
	// present.
	const char *comment(void);
	// Some key file formats allow a comment to be included. This is the comment if
	// present.
	void put_Comment(const char *newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Loads an SSH key from an OpenSSH private-key file format. If the key is
	// encrypted, set the Password property prior to calling this method.
	// 
	// Note: keyStr is not a filename -- it should contain the contents of the OpenSSH
	// formatted private key. To load from a file, you may first call LoadText to load
	// the complete contents of a text file into a string.
	// 
	bool FromOpenSshPrivateKey(const char *keyStr);

	// Loads an OpenSSH public-key from an OpenSSH formatted public-key.
	// 
	// Note: keyStr is not a filename -- it should contain the contents of the OpenSSH
	// formatted public key. To load from a file, you may first call LoadText to load
	// the complete contents of a text file into a string.
	// 
	bool FromOpenSshPublicKey(const char *keyStr);

	// Loads an SSH key from a PuTTY private-key file format. If the key is encrypted,
	// set the Password property prior to calling this method.
	// 
	// Note: keyStr is not a filename -- it should contain the contents of the PuTTY
	// formatted private key. To load from a file, you may first call LoadText to load
	// the complete contents of a text file into a string.
	// 
	bool FromPuttyPrivateKey(const char *keyStr);

	// Loads an SSH public key from an RFC 4716 format.
	// 
	// Note: keyStr is not a filename -- it should contain the contents of the PuTTY
	// formatted public key. To load from a file, you may first call LoadText to load
	// the complete contents of a text file into a string.
	// 
	bool FromRfc4716PublicKey(const char *keyStr);

	// Loads an SSH key from XML.
	// 
	// Note: xmlKey is not a filename -- it should contain the contents of the XML SSH
	// key. To load from a file, you may first call LoadText to load the complete
	// contents of a text file into a string.
	// 
	bool FromXml(const char *xmlKey);

	// Generates a fingerpring for an SSH key. A sample fingerprint looks like this:
	// ssh-dss 2048 d0:5f:f7:d6:49:60:7b:50:19:f4:41:59:d4:1f:61:7a
	bool GenFingerprint(CkString &outStr);
	// Generates a fingerpring for an SSH key. A sample fingerprint looks like this:
	// ssh-dss 2048 d0:5f:f7:d6:49:60:7b:50:19:f4:41:59:d4:1f:61:7a
	const char *genFingerprint(void);

	// Generates a new SSH DSA key that is numBits bits in length. The numBits should be at
	// least 1024 bits and a multiple of 64. Typical values are 1024 and 2048.
	bool GenerateDsaKey(int numBits);

	// Generates a new RSA public/private key pair. The numBits can range from 384 to
	// 4096. Typical key lengths are 1024 or 2048 bits. After successful generation,
	// the public/private keys can be exported in OpenSSH or PuTTY format.
	// 
	// (excerpt from Wikipedia's article on RSA) 65537 is a commonly used value for
	//  exponent. This value can be regarded as a compromise between avoiding potential
	// small exponent attacks and still allowing efficient encryptions (or signature
	// verification). The NIST Special Publication on Computer Security (SP 800-78 Rev
	// 1 of August 2007) does not allow public exponents e smaller than 65537, but does
	// not state a reason for this restriction.
	// 
	bool GenerateRsaKey(int numBits, int exponent);

	// Convenience method for loading an entire text file into an in-memory string.
	bool LoadText(const char *filename, CkString &outStr);
	// Convenience method for loading an entire text file into an in-memory string.
	const char *loadText(const char *filename);

	// Convenience method for saving the contents of a string variable to a file.
	bool SaveText(const char *strToSave, const char *filename);

	// Exports the private key to a string in OpenSSH format. If bEncrypt is true, the
	// key is encrypted using the value of the Password property. Below are samples of
	// RSA and DSA private keys in OpenSSH format, both encrypted and unencrypted:
	// 
	// Encrypted OpenSSH DSA Key
	// 
	// -----BEGIN DSA PRIVATE KEY-----
	// Proc-Type: 4,ENCRYPTED
	// DEK-Info: DES-EDE3-CBC,934E999D91EFE8F8
	// 
	// j1sslF86hdpzaBdVeLZ9akXux+CEaa6HyZgou3BkRkWpMn7gFqX3lfRIjrlhY41w
	// +snC2HNYF5ae+WymP6fQ7TOhOsQD3KFBNbohn4dJE4fB0El6OKCbR2MGJlUkwMY4
	// q3VCu3yAeiMmLwocoHxbfXYXgjqPBqbLXPSsHzpirGyC8FD8+P0QWW/W4rJGGWru
	// 866fkIPH+PCJXml4Io/YPUQYYFhRd4XTJvXqU0tLOpFfsLhetGziKNZuLGzpiVCt
	// v8Vt0VXKSD6M27gE788mEhJjtqVVpymRDReZhrt1MlGVofhuC34kA4L7+BLHARHq
	// bvM0SRSvqwNYxLNDWtV33RlOEvy/Kg7rRLnmSgZ7pPgUB4lbpC23Adp4djpdO5S2
	// LERDEV+cTmKGCpLOCGOEGlPOiEnROP8xjmqpGHNMaLh6Jd/zGdgdAN7/6Wk/TeaP
	// p/Ma4UQUnZEjkIpfxYtsMJ0Pb5JBZf41vJMVqFlDKeOngFIlWzv9G34Ip2GbZVUJ
	// 0CfbD3VrZb8t6ethcSQ7NKkMpB0s4Qu+nSIHZngb6Z/uzTVX/ElbxVu7/kca627n
	// LtXBcDZtkYey7ANePw0Q2Ju3rOcWJQK691K3M/qZkPO5TKtybmdMHV6EuVPGlPXV
	// d2fUUBCZpTs8udN9bE8lZIGC//RX4hPfCV1eL0cZehByJeJG6EK1UvrCPnsfymFi
	// bWJvjM4SWxFaMUNQRPuNywE1c9K0BqR/NQa6scADReXU66pho/ttb1Furnmm68rB
	// SQq+CA5DjBOMYIbD6s0zMLKICwrk4DYTZlhRGb/fP9BAWZN+scz3ot4RUaURasXj
	// Fb9b0arwqL5b84pDoA+gdxnDcB60RjegLTPqC/EuVepHTRRZN3w5eZvW5ZjHwmzw
	// TYPYoraj7NBRaTx81ZVkKViDZnqSY2xjUnuiUbWrgKWC+asZzQDGGebLFQmFr0V3
	// rkI7MR7JHrl5VrIKro9ewVFhqugdMASzC3e0GjKQowec5ELjVAGLz+zvHrAcrxkh
	// m8Q2lHl1efS/rnCyk9DDKqHBkcIYkyxmtQMtbg4MruEIBQhTx4bUzQdq5VLYYlqY
	// YrrEcZveySg0ILT9x0swM22ji3w0S7Xv
	// -----END DSA PRIVATE KEY-----
	// 
	// Unencrypted OpenSSH DSA Key
	// 
	// -----BEGIN DSA PRIVATE KEY-----
	// MIIDPgIBAAKCAQEAqj/5FQjdsvxasIvxdEKbIe0SuHfJxlLSdCqvKsknpn26UBSd
	// W5wXqiUSmLrU7mLIRAVxdQ7umcp3N8w/dIiVkiJvm+tUQqlPYrd6n2JK+eQ0lGtP
	// emsn6fQzHhUmG0jCFgot3nNodspb5euUxVrU0BCq2PpKRuGKt6cyq5oEAvlg1K6p
	// vpze8tZ+etZGToN2uBQ/3ysGjgiDs3Zgh/k3o+8UteuBdjd8awEHcIzFml8w3+XW
	// EjuOslWrda8KiPCRZEQIbNfiZrcTzAefNRzJhKJL6EkuCU+3oZo8DL6Xc+0Obkk6
	// 46kYV5oDYbPBDmNIQBCr6odNHnWnBBnSs3TbLQIVANkiO001oz4lzhsO/tDx9rpO
	// yH/5AoIBAA8UN/pP7CYBI1KJ5KvBM7SSd5S5ItjA2ALboHF5uAWBKpaJvaHyHi/v
	// /eCd1BahglmdTsWoP2W5p4HmHjr6fLseuPGyLTHkGFgKd/zC5eTBid8ShNPJIByK
	// m7XVGvLFhDqhiNKIIsOqYKYkNXmQjms5VInwT4GfE2orVr5MPSg3k3DtX220CIrE
	// BaPXK4JRdrq2Jezxh7Pp76w+ZEfaQhgf9uEPWtBe0zmKsQ2gjdjRphm+tl4gFR0r
	// 4JuJeOTs9UZ4rZlMojK2Ew64rHhaAROHHjOJiQdvBEBYxXNru71sqt7xQbYtqHBR
	// +oBgRFbPPouNMIUexC4DKxyNeuN2zIcCggEAXh/Akb90+cojwtyjzXTxA9h4CzfT
	// E1G0cWziwToFYPVt/xWZM1/kDEAWRWtTDidZWRxWXxP+8J7PrMwA4Pwoq2SPW1u9
	// qQh1mGpPaPDluPiRbMKL2uV9oLfVEY7naVrqH05EPgtbiNjDin7EQljo3IoKzpEK
	// B1lFHT/Vd4CMTdl7o+QhZ5ftMGv9sbmf2eZ6y9fQpebO7o4w7/LgQ5SaIWYaCiTZ
	// WNTS4vLeNBuJBVJL/pL3tMQrQFqHAg4o94q6M5Y6NvnsSoyZ3gs5bnuyH/wk2oXd
	// lNhRVx1DMYBSdeWRlgjLYUBEKDABs+1N/9nBIZDEUQYvVA71Fawp4cqizgIVAKI4
	// pSbllzsgU0NB+IQTQU7C/TKv
	// -----END DSA PRIVATE KEY-----
	// 
	// Encrypted OpenSSH RSA Key
	// 
	// -----BEGIN RSA PRIVATE KEY-----
	// Proc-Type: 4,ENCRYPTED
	// DEK-Info: DES-EDE3-CBC,DC983431B352E226
	// 
	// HtBgp1FMd8abEzUiZPbrgImaCh8f5p+IzikphBnWwAfl7ANqhsFaATs+4BoFnals
	// sdYlyYnan5I1steUqvI+J/k8j+j+6YOl93uF2nF0oBUp8RBlMprYXEALAAsuaXma
	// RAmJKQF1vmg43d4DdZTlsmogK8nzdY1E36qzSTcq/PBP+rYXDNIRIaKn35sESAIy
	// shaTOs2n8TfoxiVq0oAC5H3QkPlK3ujd77oIk7JQKEZFvE1kaPaY3a6cgGpVjP7s
	// 8eWtFQrTvLT1iqGPOiK2018Nua2rsXfxR6JBMgmkPvfgTtc31EIdfurFK1AOZfac
	// 5YXokIEVeXxChIkMFXsbgeZBK87Fa3HnSq5q8VCvuku7NPbqdSI0spGoyRrOuaO8
	// 0HXjizWg9QbH+kK5lD1ks/yR3Hj7dxoXsV0Bo8iK0NB/pGfCxwtIJYt8iC1w07Pl
	// Io59mH/e+BiHbXI8bp8mozsSvvxMQlEF/iuwaqZozhWhdsn+Q2jd4fh4qEOV49pA
	// L6utYnb64/JOqZZZe1HfKfpbNQ8ZaOjciP0+Oe+ktCtySddIYYsac/LbKHdNil78
	// EsRIzms/OpYYEt3GQBbaphZa7X2M97+qbPPO6+hosVXwUEo2Tw80gS5LMFF+8W21
	// RoK4+VqarMTqB+pHJGWpe7v5MbmKl8HG/dpBM/ufRFdLt2uGYo/dgcE9Y2trWrOP
	// DQEB9BpbQ/Od/wnhM9SsUOp3mJHgwW3waGgPoIaQdquB6ipkWen0h42XHT1EqiSZ
	// 1S5pXjZvEyXPvYP3mjXBoD0YxNftKCrFHzlWz4EtI74LSgilTLmVD2TauutDdjFc
	// zjfTtrwxnS/ZFbijZBSthhG3aVhZAmIguWhZUFJcttAZY4S/AJ46HN7qI/WmGSiQ
	// 3SJN7OCZEhJ8xidXT4giMY/xaNWaRPDpUF+aKE1vlCwtVJxV8VlqGkSyvk7/0pcg
	// CmPCH9+p4IXmGq16UNrJh6Mp+GwtTbp62mFeyzh6zo1BKPQGzvohHxLs9/qW5I9L
	// sbx34VavTbityYDj/r+UqiAJ1srn9kadpDf/Ai5emjWb/f+U5KIrrPvI+iOv/N/p
	// g3vOsDYa8x3v7bDjXpQtXxH2T2qbxqFUfI8Ckk18eE0I845TwLsAaoieFmZrYMrV
	// nsrgkALuEobUFg+3GFTTPipXhSGn5pRJb249t3pUxcrYYmq5quIBRlK2f2UsxBnX
	// 95Mv2H2Ab7ZEiN++CABoCmz3Mim8elWQxQFmW4jgseeOUCINNPxQc+sPL99HAj6s
	// C5sSkZMDKTjWYMc3UKN9XOQ1G0RbkA59jg0VwP/DDViCju/UsIllmnPTd5OpjLHf
	// AW5xq7i68AQXSXlVfy21T23GiiXKfN1b0wXRL1qvQCBYVXP9UP5zxbKLQM29aIGP
	// Sf6ccO/y+pvaxw3ujBhyDdKKMbWcKhWfteEiR2h7Nsf6aSsWha5TDr8T9GavxsyH
	// cK88J2Rp3B9Cr4I5le3rbJmi6R1gZkK89kK8UllAnaqjJJXV4EBcVs2eKxMuEsjq
	// nxCy9shNjgEOr/6VgPRrimvdhDDt4ZJynhskGoOiWFEK4Ud/1sC+NTrpwtRqjXj9
	// -----END RSA PRIVATE KEY-----
	// 
	// Unencrypted OpenSSH RSA Key
	// 
	// -----BEGIN RSA PRIVATE KEY-----
	// MIIEpQIBAAKCAQEAvTjHMg1+nAogY3o8V8zXKwf+VuTMtIZggCiUQ9NU884PeHnb
	// tezsx5KRuAtNqhWtgEsNg0/aHv6o/7E4HJE9PCqVpUXnTV1mZE14BvmusCnuz1HL
	// nV3CtSW3BOh98sHixwu0I4y2RZy0sfaTBn1zDXvCv2mrVL5D1QnOZ8B/Vi5fceQd
	// eaLe6Uzj6vTi5MBWjzvVog/xDr0tEo4FPRmu6SUkmTzhwEiETScm5qOm9VnTr2O9
	// sCibFPmqrODGAzpj/OWIH2RMMT1B9Nl6P6JvHWZc92rqrUVZHBkSoakNWkytLN5j
	// WyxwIdCFxQvCnAKic7uOodMSiqDUPJHHlPIghwIDAQABAoIBAAK9V4JPz9beH3dk
	// cqzyfW33VTH6nFX/cr0ISE7c9F+45D4vCTfsfYHvix/mCXbTsJapIagWc9FqZdF2
	// +Xd4ayB7riguwz+x1o9YR4cRywwoLBFP9tzvjMnvkaepAQHTG+HHFUTNskfDVngC
	// wOtlvGHXZPIbU+QLvoGtMFZSOHBrs1cOfoUO1BToN1cGfgp/DuJY6WWVoNs93C8J
	// wgnH2DdosRcMDYOQsASH60hEE2JJuF7Ox+BwlcsV5AUDwIeAChQI0Aend7NL9cee
	// U8o3UXAVjABowGI9EQJYUmysir5mFs/BETlPambyvEWWNCtJsT2qrCi2Xme5BRzD
	// p5MMJ+ECgYEAzfbbQagHiP48yGiZS2j7i1UcV/0pgeamtjaxJf1njyMrkyxMFOFW
	// PfLOVLcRnx4HcqYYv9Opo8mbYlxjUUFxhMBYdyNICx8OjM7eijSjGisvAh0S20Vo
	// LvhQ0zzv1/QUdnj4jO4FWNgHYUF+mre8nX8k/s8AcudBR7n5w+MhP7cCgYEA6zCw
	// DGB8ipBQMryEWVNc0eYFFXojxlFRKjp4BRc43F0s0gyRLWGCUHTN0Z1DPKQRilo6
	// D8nGANN8BYVFDEcuFk8SCks5Xk76YfL+9uAETKv7y//AE8dyrCRk8/7kyOdMhHtq
	// 1bHsAutfxeiYrdRWyK0ZApAKdSEy+BvdVRv9hbECgYEAzcNdvlUg2gKsNMcSxpym
	// GMe5nknj6svEJ2uyRLLJf91yDgEGLSIFp7Pn4AhYiW9Vn3tCZHoQEvo5yuVjr2zC
	// /Q2wE63iroGjZpbRCp+VhnI371OeYAMSF0KqdK5/Km7E9qraHOk53E1N6iKlWepP
	// e8Tm781btG9F72NjnAhQUjcCgYEA1XB2FIVsAQQ/BAx5v+cbkZHCg185ID2j/0LY
	// sSYGAFa+2lF1X03iycl3EAg8gMgU8w43KyTegNls8EWmCCKA/NX9dUIXajMan9G6
	// +akLvdlGxjfvxQN4WikdRSHJ11mx43lt10mE+pFJdX5FMVxG9g/BZsX595qNewUu
	// tJKWXcECgYEAqHYYHyZI9dah6JVpm5UQ6kJbDPZad7uus/lmJfTgY+ZCH1sEVySL
	// UYk4TyswyUqzLfazhhQ9yZsDBmdm6jRJLYtQBo9fybpOzeyK20aFIGQTSaVFbAGG
	// YIsv2DOWk9HHr0TSG7yu/1hiMf3ol0ofpTEQpXyM8qjVuqCeqsMeJ9I=
	// -----END RSA PRIVATE KEY-----
	bool ToOpenSshPrivateKey(bool bEncrypt, CkString &outStr);
	// Exports the private key to a string in OpenSSH format. If bEncrypt is true, the
	// key is encrypted using the value of the Password property. Below are samples of
	// RSA and DSA private keys in OpenSSH format, both encrypted and unencrypted:
	// 
	// Encrypted OpenSSH DSA Key
	// 
	// -----BEGIN DSA PRIVATE KEY-----
	// Proc-Type: 4,ENCRYPTED
	// DEK-Info: DES-EDE3-CBC,934E999D91EFE8F8
	// 
	// j1sslF86hdpzaBdVeLZ9akXux+CEaa6HyZgou3BkRkWpMn7gFqX3lfRIjrlhY41w
	// +snC2HNYF5ae+WymP6fQ7TOhOsQD3KFBNbohn4dJE4fB0El6OKCbR2MGJlUkwMY4
	// q3VCu3yAeiMmLwocoHxbfXYXgjqPBqbLXPSsHzpirGyC8FD8+P0QWW/W4rJGGWru
	// 866fkIPH+PCJXml4Io/YPUQYYFhRd4XTJvXqU0tLOpFfsLhetGziKNZuLGzpiVCt
	// v8Vt0VXKSD6M27gE788mEhJjtqVVpymRDReZhrt1MlGVofhuC34kA4L7+BLHARHq
	// bvM0SRSvqwNYxLNDWtV33RlOEvy/Kg7rRLnmSgZ7pPgUB4lbpC23Adp4djpdO5S2
	// LERDEV+cTmKGCpLOCGOEGlPOiEnROP8xjmqpGHNMaLh6Jd/zGdgdAN7/6Wk/TeaP
	// p/Ma4UQUnZEjkIpfxYtsMJ0Pb5JBZf41vJMVqFlDKeOngFIlWzv9G34Ip2GbZVUJ
	// 0CfbD3VrZb8t6ethcSQ7NKkMpB0s4Qu+nSIHZngb6Z/uzTVX/ElbxVu7/kca627n
	// LtXBcDZtkYey7ANePw0Q2Ju3rOcWJQK691K3M/qZkPO5TKtybmdMHV6EuVPGlPXV
	// d2fUUBCZpTs8udN9bE8lZIGC//RX4hPfCV1eL0cZehByJeJG6EK1UvrCPnsfymFi
	// bWJvjM4SWxFaMUNQRPuNywE1c9K0BqR/NQa6scADReXU66pho/ttb1Furnmm68rB
	// SQq+CA5DjBOMYIbD6s0zMLKICwrk4DYTZlhRGb/fP9BAWZN+scz3ot4RUaURasXj
	// Fb9b0arwqL5b84pDoA+gdxnDcB60RjegLTPqC/EuVepHTRRZN3w5eZvW5ZjHwmzw
	// TYPYoraj7NBRaTx81ZVkKViDZnqSY2xjUnuiUbWrgKWC+asZzQDGGebLFQmFr0V3
	// rkI7MR7JHrl5VrIKro9ewVFhqugdMASzC3e0GjKQowec5ELjVAGLz+zvHrAcrxkh
	// m8Q2lHl1efS/rnCyk9DDKqHBkcIYkyxmtQMtbg4MruEIBQhTx4bUzQdq5VLYYlqY
	// YrrEcZveySg0ILT9x0swM22ji3w0S7Xv
	// -----END DSA PRIVATE KEY-----
	// 
	// Unencrypted OpenSSH DSA Key
	// 
	// -----BEGIN DSA PRIVATE KEY-----
	// MIIDPgIBAAKCAQEAqj/5FQjdsvxasIvxdEKbIe0SuHfJxlLSdCqvKsknpn26UBSd
	// W5wXqiUSmLrU7mLIRAVxdQ7umcp3N8w/dIiVkiJvm+tUQqlPYrd6n2JK+eQ0lGtP
	// emsn6fQzHhUmG0jCFgot3nNodspb5euUxVrU0BCq2PpKRuGKt6cyq5oEAvlg1K6p
	// vpze8tZ+etZGToN2uBQ/3ysGjgiDs3Zgh/k3o+8UteuBdjd8awEHcIzFml8w3+XW
	// EjuOslWrda8KiPCRZEQIbNfiZrcTzAefNRzJhKJL6EkuCU+3oZo8DL6Xc+0Obkk6
	// 46kYV5oDYbPBDmNIQBCr6odNHnWnBBnSs3TbLQIVANkiO001oz4lzhsO/tDx9rpO
	// yH/5AoIBAA8UN/pP7CYBI1KJ5KvBM7SSd5S5ItjA2ALboHF5uAWBKpaJvaHyHi/v
	// /eCd1BahglmdTsWoP2W5p4HmHjr6fLseuPGyLTHkGFgKd/zC5eTBid8ShNPJIByK
	// m7XVGvLFhDqhiNKIIsOqYKYkNXmQjms5VInwT4GfE2orVr5MPSg3k3DtX220CIrE
	// BaPXK4JRdrq2Jezxh7Pp76w+ZEfaQhgf9uEPWtBe0zmKsQ2gjdjRphm+tl4gFR0r
	// 4JuJeOTs9UZ4rZlMojK2Ew64rHhaAROHHjOJiQdvBEBYxXNru71sqt7xQbYtqHBR
	// +oBgRFbPPouNMIUexC4DKxyNeuN2zIcCggEAXh/Akb90+cojwtyjzXTxA9h4CzfT
	// E1G0cWziwToFYPVt/xWZM1/kDEAWRWtTDidZWRxWXxP+8J7PrMwA4Pwoq2SPW1u9
	// qQh1mGpPaPDluPiRbMKL2uV9oLfVEY7naVrqH05EPgtbiNjDin7EQljo3IoKzpEK
	// B1lFHT/Vd4CMTdl7o+QhZ5ftMGv9sbmf2eZ6y9fQpebO7o4w7/LgQ5SaIWYaCiTZ
	// WNTS4vLeNBuJBVJL/pL3tMQrQFqHAg4o94q6M5Y6NvnsSoyZ3gs5bnuyH/wk2oXd
	// lNhRVx1DMYBSdeWRlgjLYUBEKDABs+1N/9nBIZDEUQYvVA71Fawp4cqizgIVAKI4
	// pSbllzsgU0NB+IQTQU7C/TKv
	// -----END DSA PRIVATE KEY-----
	// 
	// Encrypted OpenSSH RSA Key
	// 
	// -----BEGIN RSA PRIVATE KEY-----
	// Proc-Type: 4,ENCRYPTED
	// DEK-Info: DES-EDE3-CBC,DC983431B352E226
	// 
	// HtBgp1FMd8abEzUiZPbrgImaCh8f5p+IzikphBnWwAfl7ANqhsFaATs+4BoFnals
	// sdYlyYnan5I1steUqvI+J/k8j+j+6YOl93uF2nF0oBUp8RBlMprYXEALAAsuaXma
	// RAmJKQF1vmg43d4DdZTlsmogK8nzdY1E36qzSTcq/PBP+rYXDNIRIaKn35sESAIy
	// shaTOs2n8TfoxiVq0oAC5H3QkPlK3ujd77oIk7JQKEZFvE1kaPaY3a6cgGpVjP7s
	// 8eWtFQrTvLT1iqGPOiK2018Nua2rsXfxR6JBMgmkPvfgTtc31EIdfurFK1AOZfac
	// 5YXokIEVeXxChIkMFXsbgeZBK87Fa3HnSq5q8VCvuku7NPbqdSI0spGoyRrOuaO8
	// 0HXjizWg9QbH+kK5lD1ks/yR3Hj7dxoXsV0Bo8iK0NB/pGfCxwtIJYt8iC1w07Pl
	// Io59mH/e+BiHbXI8bp8mozsSvvxMQlEF/iuwaqZozhWhdsn+Q2jd4fh4qEOV49pA
	// L6utYnb64/JOqZZZe1HfKfpbNQ8ZaOjciP0+Oe+ktCtySddIYYsac/LbKHdNil78
	// EsRIzms/OpYYEt3GQBbaphZa7X2M97+qbPPO6+hosVXwUEo2Tw80gS5LMFF+8W21
	// RoK4+VqarMTqB+pHJGWpe7v5MbmKl8HG/dpBM/ufRFdLt2uGYo/dgcE9Y2trWrOP
	// DQEB9BpbQ/Od/wnhM9SsUOp3mJHgwW3waGgPoIaQdquB6ipkWen0h42XHT1EqiSZ
	// 1S5pXjZvEyXPvYP3mjXBoD0YxNftKCrFHzlWz4EtI74LSgilTLmVD2TauutDdjFc
	// zjfTtrwxnS/ZFbijZBSthhG3aVhZAmIguWhZUFJcttAZY4S/AJ46HN7qI/WmGSiQ
	// 3SJN7OCZEhJ8xidXT4giMY/xaNWaRPDpUF+aKE1vlCwtVJxV8VlqGkSyvk7/0pcg
	// CmPCH9+p4IXmGq16UNrJh6Mp+GwtTbp62mFeyzh6zo1BKPQGzvohHxLs9/qW5I9L
	// sbx34VavTbityYDj/r+UqiAJ1srn9kadpDf/Ai5emjWb/f+U5KIrrPvI+iOv/N/p
	// g3vOsDYa8x3v7bDjXpQtXxH2T2qbxqFUfI8Ckk18eE0I845TwLsAaoieFmZrYMrV
	// nsrgkALuEobUFg+3GFTTPipXhSGn5pRJb249t3pUxcrYYmq5quIBRlK2f2UsxBnX
	// 95Mv2H2Ab7ZEiN++CABoCmz3Mim8elWQxQFmW4jgseeOUCINNPxQc+sPL99HAj6s
	// C5sSkZMDKTjWYMc3UKN9XOQ1G0RbkA59jg0VwP/DDViCju/UsIllmnPTd5OpjLHf
	// AW5xq7i68AQXSXlVfy21T23GiiXKfN1b0wXRL1qvQCBYVXP9UP5zxbKLQM29aIGP
	// Sf6ccO/y+pvaxw3ujBhyDdKKMbWcKhWfteEiR2h7Nsf6aSsWha5TDr8T9GavxsyH
	// cK88J2Rp3B9Cr4I5le3rbJmi6R1gZkK89kK8UllAnaqjJJXV4EBcVs2eKxMuEsjq
	// nxCy9shNjgEOr/6VgPRrimvdhDDt4ZJynhskGoOiWFEK4Ud/1sC+NTrpwtRqjXj9
	// -----END RSA PRIVATE KEY-----
	// 
	// Unencrypted OpenSSH RSA Key
	// 
	// -----BEGIN RSA PRIVATE KEY-----
	// MIIEpQIBAAKCAQEAvTjHMg1+nAogY3o8V8zXKwf+VuTMtIZggCiUQ9NU884PeHnb
	// tezsx5KRuAtNqhWtgEsNg0/aHv6o/7E4HJE9PCqVpUXnTV1mZE14BvmusCnuz1HL
	// nV3CtSW3BOh98sHixwu0I4y2RZy0sfaTBn1zDXvCv2mrVL5D1QnOZ8B/Vi5fceQd
	// eaLe6Uzj6vTi5MBWjzvVog/xDr0tEo4FPRmu6SUkmTzhwEiETScm5qOm9VnTr2O9
	// sCibFPmqrODGAzpj/OWIH2RMMT1B9Nl6P6JvHWZc92rqrUVZHBkSoakNWkytLN5j
	// WyxwIdCFxQvCnAKic7uOodMSiqDUPJHHlPIghwIDAQABAoIBAAK9V4JPz9beH3dk
	// cqzyfW33VTH6nFX/cr0ISE7c9F+45D4vCTfsfYHvix/mCXbTsJapIagWc9FqZdF2
	// +Xd4ayB7riguwz+x1o9YR4cRywwoLBFP9tzvjMnvkaepAQHTG+HHFUTNskfDVngC
	// wOtlvGHXZPIbU+QLvoGtMFZSOHBrs1cOfoUO1BToN1cGfgp/DuJY6WWVoNs93C8J
	// wgnH2DdosRcMDYOQsASH60hEE2JJuF7Ox+BwlcsV5AUDwIeAChQI0Aend7NL9cee
	// U8o3UXAVjABowGI9EQJYUmysir5mFs/BETlPambyvEWWNCtJsT2qrCi2Xme5BRzD
	// p5MMJ+ECgYEAzfbbQagHiP48yGiZS2j7i1UcV/0pgeamtjaxJf1njyMrkyxMFOFW
	// PfLOVLcRnx4HcqYYv9Opo8mbYlxjUUFxhMBYdyNICx8OjM7eijSjGisvAh0S20Vo
	// LvhQ0zzv1/QUdnj4jO4FWNgHYUF+mre8nX8k/s8AcudBR7n5w+MhP7cCgYEA6zCw
	// DGB8ipBQMryEWVNc0eYFFXojxlFRKjp4BRc43F0s0gyRLWGCUHTN0Z1DPKQRilo6
	// D8nGANN8BYVFDEcuFk8SCks5Xk76YfL+9uAETKv7y//AE8dyrCRk8/7kyOdMhHtq
	// 1bHsAutfxeiYrdRWyK0ZApAKdSEy+BvdVRv9hbECgYEAzcNdvlUg2gKsNMcSxpym
	// GMe5nknj6svEJ2uyRLLJf91yDgEGLSIFp7Pn4AhYiW9Vn3tCZHoQEvo5yuVjr2zC
	// /Q2wE63iroGjZpbRCp+VhnI371OeYAMSF0KqdK5/Km7E9qraHOk53E1N6iKlWepP
	// e8Tm781btG9F72NjnAhQUjcCgYEA1XB2FIVsAQQ/BAx5v+cbkZHCg185ID2j/0LY
	// sSYGAFa+2lF1X03iycl3EAg8gMgU8w43KyTegNls8EWmCCKA/NX9dUIXajMan9G6
	// +akLvdlGxjfvxQN4WikdRSHJ11mx43lt10mE+pFJdX5FMVxG9g/BZsX595qNewUu
	// tJKWXcECgYEAqHYYHyZI9dah6JVpm5UQ6kJbDPZad7uus/lmJfTgY+ZCH1sEVySL
	// UYk4TyswyUqzLfazhhQ9yZsDBmdm6jRJLYtQBo9fybpOzeyK20aFIGQTSaVFbAGG
	// YIsv2DOWk9HHr0TSG7yu/1hiMf3ol0ofpTEQpXyM8qjVuqCeqsMeJ9I=
	// -----END RSA PRIVATE KEY-----
	const char *toOpenSshPrivateKey(bool bEncrypt);

	// Exports the public key to a string in OpenSSH format. Sample OpenSSH-formatted
	// DSA and RSA public keys are below:
	// 
	// OpenSSH RSA Public Key (Line-breaks are added for readability. An
	// OpenSSH-formatted public key would typically be a single line, not multiple
	// lines.)
	// 
	// ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC9OMcyDX6cCiBjejxXzNcrB/5W5My0hmCAKJRD01Tzzg94edu17O
	// zHkpG4C02qFa2ASw2DT9oe/qj/sTgckT08KpWlRedNXWZkTXgG+a6wKe7PUcudXcK1JbcE6H3yweLHC7QjjL
	// ZFnLSx9pMGfXMNe8K/aatUvkPVCc5nwH9WLl9x5B15ot7pTOPq9OLkwFaPO9WiD/EOvS0SjgU9Ga7pJSSZP
	// OHASIRNJybmo6b1WdOvY72wKJsU+aqs4MYDOmP85YgfZEwxPUH02Xo/om8dZlz3auqtRVkcGRKhqQ1aTK0
	// s3mNbLHAh0IXFC8KcAqJzu46h0xKKoNQ8kceU8iCH 
	// 
	// OpenSSH DSA Public Key (Line-breaks are added for readability.)
	// 
	// ssh-dss AAAAB3NzaC1kc3MAAAEBAKo/+RUI3bL8WrCL8XRCmyHtErh3ycZS0nQqryrJJ6Z9ulAUnVucF6olEpi61O5iyEQF
	// cXUO7pnKdzfMP3SIlZIib5vrVEKpT2K3ep9iSvnkNJRrT3prJ+n0Mx4VJhtIwhYKLd5zaHbKW+XrlMVa1NAQqtj6Skb
	// hirenMquaBAL5YNSuqb6c3vLWfnrWRk6DdrgUP98rBo4Ig7N2YIf5N6PvFLXrgXY3fGsBB3CMxZpfMN/l1hI7jrJVq3
	// WvCojwkWRECGzX4ma3E8wHnzUcyYSiS+hJLglPt6GaPAy+l3PtDm5JOuOpGFeaA2GzwQ5jSEAQq+qHTR51pwQ
	// Z0rN02y0AAAAVANkiO001oz4lzhsO/tDx9rpOyH/5AAABAA8UN/pP7CYBI1KJ5KvBM7SSd5S5ItjA2ALboHF5uAWB
	// KpaJvaHyHi/v/eCd1BahglmdTsWoP2W5p4HmHjr6fLseuPGyLTHkGFgKd/zC5eTBid8ShNPJIByKm7XVGvLFhDqhiN
	// KIIsOqYKYkNXmQjms5VInwT4GfE2orVr5MPSg3k3DtX220CIrEBaPXK4JRdrq2Jezxh7Pp76w+ZEfaQhgf9uEPWtBe0
	// zmKsQ2gjdjRphm+tl4gFR0r4JuJeOTs9UZ4rZlMojK2Ew64rHhaAROHHjOJiQdvBEBYxXNru71sqt7xQbYtqHBR+oBgR
	// FbPPouNMIUexC4DKxyNeuN2zIcAAAEAXh/Akb90+cojwtyjzXTxA9h4CzfTE1G0cWziwToFYPVt/xWZM1/kDEAWR
	// WtTDidZWRxWXxP+8J7PrMwA4Pwoq2SPW1u9qQh1mGpPaPDluPiRbMKL2uV9oLfVEY7naVrqH05EPgtbiNjDin7E
	// Qljo3IoKzpEKB1lFHT/Vd4CMTdl7o+QhZ5ftMGv9sbmf2eZ6y9fQpebO7o4w7/LgQ5SaIWYaCiTZWNTS4vLeNBuJB
	// VJL/pL3tMQrQFqHAg4o94q6M5Y6NvnsSoyZ3gs5bnuyH/wk2oXdlNhRVx1DMYBSdeWRlgjLYUBEKDABs+1N/9nB
	// IZDEUQYvVA71Fawp4cqizg== 
	bool ToOpenSshPublicKey(CkString &outStr);
	// Exports the public key to a string in OpenSSH format. Sample OpenSSH-formatted
	// DSA and RSA public keys are below:
	// 
	// OpenSSH RSA Public Key (Line-breaks are added for readability. An
	// OpenSSH-formatted public key would typically be a single line, not multiple
	// lines.)
	// 
	// ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC9OMcyDX6cCiBjejxXzNcrB/5W5My0hmCAKJRD01Tzzg94edu17O
	// zHkpG4C02qFa2ASw2DT9oe/qj/sTgckT08KpWlRedNXWZkTXgG+a6wKe7PUcudXcK1JbcE6H3yweLHC7QjjL
	// ZFnLSx9pMGfXMNe8K/aatUvkPVCc5nwH9WLl9x5B15ot7pTOPq9OLkwFaPO9WiD/EOvS0SjgU9Ga7pJSSZP
	// OHASIRNJybmo6b1WdOvY72wKJsU+aqs4MYDOmP85YgfZEwxPUH02Xo/om8dZlz3auqtRVkcGRKhqQ1aTK0
	// s3mNbLHAh0IXFC8KcAqJzu46h0xKKoNQ8kceU8iCH 
	// 
	// OpenSSH DSA Public Key (Line-breaks are added for readability.)
	// 
	// ssh-dss AAAAB3NzaC1kc3MAAAEBAKo/+RUI3bL8WrCL8XRCmyHtErh3ycZS0nQqryrJJ6Z9ulAUnVucF6olEpi61O5iyEQF
	// cXUO7pnKdzfMP3SIlZIib5vrVEKpT2K3ep9iSvnkNJRrT3prJ+n0Mx4VJhtIwhYKLd5zaHbKW+XrlMVa1NAQqtj6Skb
	// hirenMquaBAL5YNSuqb6c3vLWfnrWRk6DdrgUP98rBo4Ig7N2YIf5N6PvFLXrgXY3fGsBB3CMxZpfMN/l1hI7jrJVq3
	// WvCojwkWRECGzX4ma3E8wHnzUcyYSiS+hJLglPt6GaPAy+l3PtDm5JOuOpGFeaA2GzwQ5jSEAQq+qHTR51pwQ
	// Z0rN02y0AAAAVANkiO001oz4lzhsO/tDx9rpOyH/5AAABAA8UN/pP7CYBI1KJ5KvBM7SSd5S5ItjA2ALboHF5uAWB
	// KpaJvaHyHi/v/eCd1BahglmdTsWoP2W5p4HmHjr6fLseuPGyLTHkGFgKd/zC5eTBid8ShNPJIByKm7XVGvLFhDqhiN
	// KIIsOqYKYkNXmQjms5VInwT4GfE2orVr5MPSg3k3DtX220CIrEBaPXK4JRdrq2Jezxh7Pp76w+ZEfaQhgf9uEPWtBe0
	// zmKsQ2gjdjRphm+tl4gFR0r4JuJeOTs9UZ4rZlMojK2Ew64rHhaAROHHjOJiQdvBEBYxXNru71sqt7xQbYtqHBR+oBgR
	// FbPPouNMIUexC4DKxyNeuN2zIcAAAEAXh/Akb90+cojwtyjzXTxA9h4CzfTE1G0cWziwToFYPVt/xWZM1/kDEAWR
	// WtTDidZWRxWXxP+8J7PrMwA4Pwoq2SPW1u9qQh1mGpPaPDluPiRbMKL2uV9oLfVEY7naVrqH05EPgtbiNjDin7E
	// Qljo3IoKzpEKB1lFHT/Vd4CMTdl7o+QhZ5ftMGv9sbmf2eZ6y9fQpebO7o4w7/LgQ5SaIWYaCiTZWNTS4vLeNBuJB
	// VJL/pL3tMQrQFqHAg4o94q6M5Y6NvnsSoyZ3gs5bnuyH/wk2oXdlNhRVx1DMYBSdeWRlgjLYUBEKDABs+1N/9nB
	// IZDEUQYvVA71Fawp4cqizg== 
	const char *toOpenSshPublicKey(void);

	// Exports the private key to PuTTY format. If bEncrypt is true, the key is encrypted
	// using the value of the Password property. Below are samples of RSA and DSA
	// private keys in PuTTY format, both encrypted and unencrypted:
	// 
	// Encrypted PuTTY DSA Key
	// 
	// PuTTY-User-Key-File-2: ssh-dss
	// Encryption: aes256-cbc
	// Comment: dss-key-20080914
	// Public-Lines: 18
	// AAAAB3NzaC1kc3MAAAEBAKo/+RUI3bL8WrCL8XRCmyHtErh3ycZS0nQqryrJJ6Z9
	// ulAUnVucF6olEpi61O5iyEQFcXUO7pnKdzfMP3SIlZIib5vrVEKpT2K3ep9iSvnk
	// NJRrT3prJ+n0Mx4VJhtIwhYKLd5zaHbKW+XrlMVa1NAQqtj6SkbhirenMquaBAL5
	// YNSuqb6c3vLWfnrWRk6DdrgUP98rBo4Ig7N2YIf5N6PvFLXrgXY3fGsBB3CMxZpf
	// MN/l1hI7jrJVq3WvCojwkWRECGzX4ma3E8wHnzUcyYSiS+hJLglPt6GaPAy+l3Pt
	// Dm5JOuOpGFeaA2GzwQ5jSEAQq+qHTR51pwQZ0rN02y0AAAAVANkiO001oz4lzhsO
	// /tDx9rpOyH/5AAABAA8UN/pP7CYBI1KJ5KvBM7SSd5S5ItjA2ALboHF5uAWBKpaJ
	// vaHyHi/v/eCd1BahglmdTsWoP2W5p4HmHjr6fLseuPGyLTHkGFgKd/zC5eTBid8S
	// hNPJIByKm7XVGvLFhDqhiNKIIsOqYKYkNXmQjms5VInwT4GfE2orVr5MPSg3k3Dt
	// X220CIrEBaPXK4JRdrq2Jezxh7Pp76w+ZEfaQhgf9uEPWtBe0zmKsQ2gjdjRphm+
	// tl4gFR0r4JuJeOTs9UZ4rZlMojK2Ew64rHhaAROHHjOJiQdvBEBYxXNru71sqt7x
	// QbYtqHBR+oBgRFbPPouNMIUexC4DKxyNeuN2zIcAAAEAXh/Akb90+cojwtyjzXTx
	// A9h4CzfTE1G0cWziwToFYPVt/xWZM1/kDEAWRWtTDidZWRxWXxP+8J7PrMwA4Pwo
	// q2SPW1u9qQh1mGpPaPDluPiRbMKL2uV9oLfVEY7naVrqH05EPgtbiNjDin7EQljo
	// 3IoKzpEKB1lFHT/Vd4CMTdl7o+QhZ5ftMGv9sbmf2eZ6y9fQpebO7o4w7/LgQ5Sa
	// IWYaCiTZWNTS4vLeNBuJBVJL/pL3tMQrQFqHAg4o94q6M5Y6NvnsSoyZ3gs5bnuy
	// H/wk2oXdlNhRVx1DMYBSdeWRlgjLYUBEKDABs+1N/9nBIZDEUQYvVA71Fawp4cqi
	// zg==
	// Private-Lines: 1
	// L2iyk4xEmQi4IK8ZCm35/UDan0VVUdd9whyljTo6SoA=
	// Private-MAC: 5633A6762B5D9016A5B1D7753713C4A13CD734EC
	// 
	// Unencrypted PuTTY DSA Key
	// 
	// PuTTY-User-Key-File-2: ssh-dss
	// Encryption: none
	// Comment: dss-key-20080914
	// Public-Lines: 18
	// AAAAB3NzaC1kc3MAAAEBAKo/+RUI3bL8WrCL8XRCmyHtErh3ycZS0nQqryrJJ6Z9
	// ulAUnVucF6olEpi61O5iyEQFcXUO7pnKdzfMP3SIlZIib5vrVEKpT2K3ep9iSvnk
	// NJRrT3prJ+n0Mx4VJhtIwhYKLd5zaHbKW+XrlMVa1NAQqtj6SkbhirenMquaBAL5
	// YNSuqb6c3vLWfnrWRk6DdrgUP98rBo4Ig7N2YIf5N6PvFLXrgXY3fGsBB3CMxZpf
	// MN/l1hI7jrJVq3WvCojwkWRECGzX4ma3E8wHnzUcyYSiS+hJLglPt6GaPAy+l3Pt
	// Dm5JOuOpGFeaA2GzwQ5jSEAQq+qHTR51pwQZ0rN02y0AAAAVANkiO001oz4lzhsO
	// /tDx9rpOyH/5AAABAA8UN/pP7CYBI1KJ5KvBM7SSd5S5ItjA2ALboHF5uAWBKpaJ
	// vaHyHi/v/eCd1BahglmdTsWoP2W5p4HmHjr6fLseuPGyLTHkGFgKd/zC5eTBid8S
	// hNPJIByKm7XVGvLFhDqhiNKIIsOqYKYkNXmQjms5VInwT4GfE2orVr5MPSg3k3Dt
	// X220CIrEBaPXK4JRdrq2Jezxh7Pp76w+ZEfaQhgf9uEPWtBe0zmKsQ2gjdjRphm+
	// tl4gFR0r4JuJeOTs9UZ4rZlMojK2Ew64rHhaAROHHjOJiQdvBEBYxXNru71sqt7x
	// QbYtqHBR+oBgRFbPPouNMIUexC4DKxyNeuN2zIcAAAEAXh/Akb90+cojwtyjzXTx
	// A9h4CzfTE1G0cWziwToFYPVt/xWZM1/kDEAWRWtTDidZWRxWXxP+8J7PrMwA4Pwo
	// q2SPW1u9qQh1mGpPaPDluPiRbMKL2uV9oLfVEY7naVrqH05EPgtbiNjDin7EQljo
	// 3IoKzpEKB1lFHT/Vd4CMTdl7o+QhZ5ftMGv9sbmf2eZ6y9fQpebO7o4w7/LgQ5Sa
	// IWYaCiTZWNTS4vLeNBuJBVJL/pL3tMQrQFqHAg4o94q6M5Y6NvnsSoyZ3gs5bnuy
	// H/wk2oXdlNhRVx1DMYBSdeWRlgjLYUBEKDABs+1N/9nBIZDEUQYvVA71Fawp4cqi
	// zg==
	// Private-Lines: 1
	// AAAAFQCiOKUm5Zc7IFNDQfiEE0FOwv0yrw==
	// Private-MAC: 7A13473EBD5C8E1ADD57D8AE5A5EF3F1789DDD8B
	// 
	// Encrypted PuTTY RSA Key
	// 
	// PuTTY-User-Key-File-2: ssh-rsa
	// Encryption: aes256-cbc
	// Comment: rsa-key-20080914
	// Public-Lines: 6
	// AAAAB3NzaC1yc2EAAAADAQABAAABAQC9OMcyDX6cCiBjejxXzNcrB/5W5My0hmCA
	// KJRD01Tzzg94edu17OzHkpG4C02qFa2ASw2DT9oe/qj/sTgckT08KpWlRedNXWZk
	// TXgG+a6wKe7PUcudXcK1JbcE6H3yweLHC7QjjLZFnLSx9pMGfXMNe8K/aatUvkPV
	// Cc5nwH9WLl9x5B15ot7pTOPq9OLkwFaPO9WiD/EOvS0SjgU9Ga7pJSSZPOHASIRN
	// Jybmo6b1WdOvY72wKJsU+aqs4MYDOmP85YgfZEwxPUH02Xo/om8dZlz3auqtRVkc
	// GRKhqQ1aTK0s3mNbLHAh0IXFC8KcAqJzu46h0xKKoNQ8kceU8iCH
	// Private-Lines: 14
	// SC03/1Szexogtlxrde1JbuWD0z9cNRLGlroT9Vf7uAJLRLF4bM7ovlcKPVrtsymG
	// lJitt6bhK9bmFIGj3Ko+RImpwFNHlL56UYtnJqx2ihSTbU8JJ34VyLy7ADjUlg3S
	// wL4YY2Uxwo24p0oLTyl3UYBVZ9z+e9fqTiZJI+jl1mFSJz2FbuA2iuiGZQcOKKHx
	// cLye5GgoXgSrMjNcm6evVmHrAi+RjakhBjzFm/aCKmiJr9bf12WI+Sp06oOvnShP
	// WcrrqVsr1+4Ju85z2wFevHYfFLjqW13AAMMdJnr/4x0g/AjTjxiuADlXBC5d+V1p
	// 7Kwi9ZDd2NMeSeTeM9ZIsKpuR2ndURYsgbTe/Hhl1nU2VwzZJuNU58ROZzjb3HOv
	// kjk+2GZhAqVQSfhm8ma9SBW9YIfIRawKSQZh1v9Sa7jh68WjeCzndvyzUCbl0tiW
	// irSTLScKcyfxRkZ952CMgGsqKX6kDdb2Yz4YLxSdltD9qs/aPyweln+uf7m+5hIV
	// SpFXw7gjZjMq+US5fstdhTAZjkzkKyV4ejlfekneIDmq5oXR5kf+3y4oqkP4nWS8
	// xP6aURBK7GdKxREmnj2E5He7nQNf5d5fGLMGtvULbY18k6aMVqRLbCt1rwF7lVsz
	// dRrp8fX94pdu0ZFLb8QwKnDI7hXzrK0lYnpfIECwPlA9SdhChVRXsvUFi5scTdB0
	// /2lV+IVcz0e1GtbsvV5EaFCcXy9k5Rq8zjhfbbTWtur7dJbwKPPQyByioNeGwzvT
	// LN0X5hn0AaDjbSH6KNPFY0YPxx4rfqsVqI8jH4CJJhoByEtb5cBJFpLhl9o6t27d
	// 3oafNIL7UjVL0j5df+4JPjSbwm+44ccrp2FBRnIJw5MKtKz58lBarpwKHRecw6Yk
	// Private-MAC: 743AFA7F93ABE10BF61C21B9BDEFDCBF52F04980
	// 
	// Unencrypted PuTTY RSA Key
	// 
	// PuTTY-User-Key-File-2: ssh-rsa
	// Encryption: none
	// Comment: rsa-key-20080914
	// Public-Lines: 6
	// AAAAB3NzaC1yc2EAAAADAQABAAABAQC9OMcyDX6cCiBjejxXzNcrB/5W5My0hmCA
	// KJRD01Tzzg94edu17OzHkpG4C02qFa2ASw2DT9oe/qj/sTgckT08KpWlRedNXWZk
	// TXgG+a6wKe7PUcudXcK1JbcE6H3yweLHC7QjjLZFnLSx9pMGfXMNe8K/aatUvkPV
	// Cc5nwH9WLl9x5B15ot7pTOPq9OLkwFaPO9WiD/EOvS0SjgU9Ga7pJSSZPOHASIRN
	// Jybmo6b1WdOvY72wKJsU+aqs4MYDOmP85YgfZEwxPUH02Xo/om8dZlz3auqtRVkc
	// GRKhqQ1aTK0s3mNbLHAh0IXFC8KcAqJzu46h0xKKoNQ8kceU8iCH
	// Private-Lines: 14
	// AAABAAK9V4JPz9beH3dkcqzyfW33VTH6nFX/cr0ISE7c9F+45D4vCTfsfYHvix/m
	// CXbTsJapIagWc9FqZdF2+Xd4ayB7riguwz+x1o9YR4cRywwoLBFP9tzvjMnvkaep
	// AQHTG+HHFUTNskfDVngCwOtlvGHXZPIbU+QLvoGtMFZSOHBrs1cOfoUO1BToN1cG
	// fgp/DuJY6WWVoNs93C8JwgnH2DdosRcMDYOQsASH60hEE2JJuF7Ox+BwlcsV5AUD
	// wIeAChQI0Aend7NL9ceeU8o3UXAVjABowGI9EQJYUmysir5mFs/BETlPambyvEWW
	// NCtJsT2qrCi2Xme5BRzDp5MMJ+EAAACBAM3220GoB4j+PMhomUto+4tVHFf9KYHm
	// prY2sSX9Z48jK5MsTBThVj3yzlS3EZ8eB3KmGL/TqaPJm2JcY1FBcYTAWHcjSAsf
	// DozO3oo0oxorLwIdEttFaC74UNM879f0FHZ4+IzuBVjYB2FBfpq3vJ1/JP7PAHLn
	// QUe5+cPjIT+3AAAAgQDrMLAMYHyKkFAyvIRZU1zR5gUVeiPGUVEqOngFFzjcXSzS
	// DJEtYYJQdM3RnUM8pBGKWjoPycYA03wFhUUMRy4WTxIKSzleTvph8v724ARMq/vL
	// /8ATx3KsJGTz/uTI50yEe2rVsewC61/F6Jit1FbIrRkCkAp1ITL4G91VG/2FsQAA
	// AIEAqHYYHyZI9dah6JVpm5UQ6kJbDPZad7uus/lmJfTgY+ZCH1sEVySLUYk4Tysw
	// yUqzLfazhhQ9yZsDBmdm6jRJLYtQBo9fybpOzeyK20aFIGQTSaVFbAGGYIsv2DOW
	// k9HHr0TSG7yu/1hiMf3ol0ofpTEQpXyM8qjVuqCeqsMeJ9I=
	// Private-MAC: 3BE4CAA1B2AE19C3E6841639BD7275019CF961F1
	bool ToPuttyPrivateKey(bool bEncrypt, CkString &outStr);
	// Exports the private key to PuTTY format. If bEncrypt is true, the key is encrypted
	// using the value of the Password property. Below are samples of RSA and DSA
	// private keys in PuTTY format, both encrypted and unencrypted:
	// 
	// Encrypted PuTTY DSA Key
	// 
	// PuTTY-User-Key-File-2: ssh-dss
	// Encryption: aes256-cbc
	// Comment: dss-key-20080914
	// Public-Lines: 18
	// AAAAB3NzaC1kc3MAAAEBAKo/+RUI3bL8WrCL8XRCmyHtErh3ycZS0nQqryrJJ6Z9
	// ulAUnVucF6olEpi61O5iyEQFcXUO7pnKdzfMP3SIlZIib5vrVEKpT2K3ep9iSvnk
	// NJRrT3prJ+n0Mx4VJhtIwhYKLd5zaHbKW+XrlMVa1NAQqtj6SkbhirenMquaBAL5
	// YNSuqb6c3vLWfnrWRk6DdrgUP98rBo4Ig7N2YIf5N6PvFLXrgXY3fGsBB3CMxZpf
	// MN/l1hI7jrJVq3WvCojwkWRECGzX4ma3E8wHnzUcyYSiS+hJLglPt6GaPAy+l3Pt
	// Dm5JOuOpGFeaA2GzwQ5jSEAQq+qHTR51pwQZ0rN02y0AAAAVANkiO001oz4lzhsO
	// /tDx9rpOyH/5AAABAA8UN/pP7CYBI1KJ5KvBM7SSd5S5ItjA2ALboHF5uAWBKpaJ
	// vaHyHi/v/eCd1BahglmdTsWoP2W5p4HmHjr6fLseuPGyLTHkGFgKd/zC5eTBid8S
	// hNPJIByKm7XVGvLFhDqhiNKIIsOqYKYkNXmQjms5VInwT4GfE2orVr5MPSg3k3Dt
	// X220CIrEBaPXK4JRdrq2Jezxh7Pp76w+ZEfaQhgf9uEPWtBe0zmKsQ2gjdjRphm+
	// tl4gFR0r4JuJeOTs9UZ4rZlMojK2Ew64rHhaAROHHjOJiQdvBEBYxXNru71sqt7x
	// QbYtqHBR+oBgRFbPPouNMIUexC4DKxyNeuN2zIcAAAEAXh/Akb90+cojwtyjzXTx
	// A9h4CzfTE1G0cWziwToFYPVt/xWZM1/kDEAWRWtTDidZWRxWXxP+8J7PrMwA4Pwo
	// q2SPW1u9qQh1mGpPaPDluPiRbMKL2uV9oLfVEY7naVrqH05EPgtbiNjDin7EQljo
	// 3IoKzpEKB1lFHT/Vd4CMTdl7o+QhZ5ftMGv9sbmf2eZ6y9fQpebO7o4w7/LgQ5Sa
	// IWYaCiTZWNTS4vLeNBuJBVJL/pL3tMQrQFqHAg4o94q6M5Y6NvnsSoyZ3gs5bnuy
	// H/wk2oXdlNhRVx1DMYBSdeWRlgjLYUBEKDABs+1N/9nBIZDEUQYvVA71Fawp4cqi
	// zg==
	// Private-Lines: 1
	// L2iyk4xEmQi4IK8ZCm35/UDan0VVUdd9whyljTo6SoA=
	// Private-MAC: 5633A6762B5D9016A5B1D7753713C4A13CD734EC
	// 
	// Unencrypted PuTTY DSA Key
	// 
	// PuTTY-User-Key-File-2: ssh-dss
	// Encryption: none
	// Comment: dss-key-20080914
	// Public-Lines: 18
	// AAAAB3NzaC1kc3MAAAEBAKo/+RUI3bL8WrCL8XRCmyHtErh3ycZS0nQqryrJJ6Z9
	// ulAUnVucF6olEpi61O5iyEQFcXUO7pnKdzfMP3SIlZIib5vrVEKpT2K3ep9iSvnk
	// NJRrT3prJ+n0Mx4VJhtIwhYKLd5zaHbKW+XrlMVa1NAQqtj6SkbhirenMquaBAL5
	// YNSuqb6c3vLWfnrWRk6DdrgUP98rBo4Ig7N2YIf5N6PvFLXrgXY3fGsBB3CMxZpf
	// MN/l1hI7jrJVq3WvCojwkWRECGzX4ma3E8wHnzUcyYSiS+hJLglPt6GaPAy+l3Pt
	// Dm5JOuOpGFeaA2GzwQ5jSEAQq+qHTR51pwQZ0rN02y0AAAAVANkiO001oz4lzhsO
	// /tDx9rpOyH/5AAABAA8UN/pP7CYBI1KJ5KvBM7SSd5S5ItjA2ALboHF5uAWBKpaJ
	// vaHyHi/v/eCd1BahglmdTsWoP2W5p4HmHjr6fLseuPGyLTHkGFgKd/zC5eTBid8S
	// hNPJIByKm7XVGvLFhDqhiNKIIsOqYKYkNXmQjms5VInwT4GfE2orVr5MPSg3k3Dt
	// X220CIrEBaPXK4JRdrq2Jezxh7Pp76w+ZEfaQhgf9uEPWtBe0zmKsQ2gjdjRphm+
	// tl4gFR0r4JuJeOTs9UZ4rZlMojK2Ew64rHhaAROHHjOJiQdvBEBYxXNru71sqt7x
	// QbYtqHBR+oBgRFbPPouNMIUexC4DKxyNeuN2zIcAAAEAXh/Akb90+cojwtyjzXTx
	// A9h4CzfTE1G0cWziwToFYPVt/xWZM1/kDEAWRWtTDidZWRxWXxP+8J7PrMwA4Pwo
	// q2SPW1u9qQh1mGpPaPDluPiRbMKL2uV9oLfVEY7naVrqH05EPgtbiNjDin7EQljo
	// 3IoKzpEKB1lFHT/Vd4CMTdl7o+QhZ5ftMGv9sbmf2eZ6y9fQpebO7o4w7/LgQ5Sa
	// IWYaCiTZWNTS4vLeNBuJBVJL/pL3tMQrQFqHAg4o94q6M5Y6NvnsSoyZ3gs5bnuy
	// H/wk2oXdlNhRVx1DMYBSdeWRlgjLYUBEKDABs+1N/9nBIZDEUQYvVA71Fawp4cqi
	// zg==
	// Private-Lines: 1
	// AAAAFQCiOKUm5Zc7IFNDQfiEE0FOwv0yrw==
	// Private-MAC: 7A13473EBD5C8E1ADD57D8AE5A5EF3F1789DDD8B
	// 
	// Encrypted PuTTY RSA Key
	// 
	// PuTTY-User-Key-File-2: ssh-rsa
	// Encryption: aes256-cbc
	// Comment: rsa-key-20080914
	// Public-Lines: 6
	// AAAAB3NzaC1yc2EAAAADAQABAAABAQC9OMcyDX6cCiBjejxXzNcrB/5W5My0hmCA
	// KJRD01Tzzg94edu17OzHkpG4C02qFa2ASw2DT9oe/qj/sTgckT08KpWlRedNXWZk
	// TXgG+a6wKe7PUcudXcK1JbcE6H3yweLHC7QjjLZFnLSx9pMGfXMNe8K/aatUvkPV
	// Cc5nwH9WLl9x5B15ot7pTOPq9OLkwFaPO9WiD/EOvS0SjgU9Ga7pJSSZPOHASIRN
	// Jybmo6b1WdOvY72wKJsU+aqs4MYDOmP85YgfZEwxPUH02Xo/om8dZlz3auqtRVkc
	// GRKhqQ1aTK0s3mNbLHAh0IXFC8KcAqJzu46h0xKKoNQ8kceU8iCH
	// Private-Lines: 14
	// SC03/1Szexogtlxrde1JbuWD0z9cNRLGlroT9Vf7uAJLRLF4bM7ovlcKPVrtsymG
	// lJitt6bhK9bmFIGj3Ko+RImpwFNHlL56UYtnJqx2ihSTbU8JJ34VyLy7ADjUlg3S
	// wL4YY2Uxwo24p0oLTyl3UYBVZ9z+e9fqTiZJI+jl1mFSJz2FbuA2iuiGZQcOKKHx
	// cLye5GgoXgSrMjNcm6evVmHrAi+RjakhBjzFm/aCKmiJr9bf12WI+Sp06oOvnShP
	// WcrrqVsr1+4Ju85z2wFevHYfFLjqW13AAMMdJnr/4x0g/AjTjxiuADlXBC5d+V1p
	// 7Kwi9ZDd2NMeSeTeM9ZIsKpuR2ndURYsgbTe/Hhl1nU2VwzZJuNU58ROZzjb3HOv
	// kjk+2GZhAqVQSfhm8ma9SBW9YIfIRawKSQZh1v9Sa7jh68WjeCzndvyzUCbl0tiW
	// irSTLScKcyfxRkZ952CMgGsqKX6kDdb2Yz4YLxSdltD9qs/aPyweln+uf7m+5hIV
	// SpFXw7gjZjMq+US5fstdhTAZjkzkKyV4ejlfekneIDmq5oXR5kf+3y4oqkP4nWS8
	// xP6aURBK7GdKxREmnj2E5He7nQNf5d5fGLMGtvULbY18k6aMVqRLbCt1rwF7lVsz
	// dRrp8fX94pdu0ZFLb8QwKnDI7hXzrK0lYnpfIECwPlA9SdhChVRXsvUFi5scTdB0
	// /2lV+IVcz0e1GtbsvV5EaFCcXy9k5Rq8zjhfbbTWtur7dJbwKPPQyByioNeGwzvT
	// LN0X5hn0AaDjbSH6KNPFY0YPxx4rfqsVqI8jH4CJJhoByEtb5cBJFpLhl9o6t27d
	// 3oafNIL7UjVL0j5df+4JPjSbwm+44ccrp2FBRnIJw5MKtKz58lBarpwKHRecw6Yk
	// Private-MAC: 743AFA7F93ABE10BF61C21B9BDEFDCBF52F04980
	// 
	// Unencrypted PuTTY RSA Key
	// 
	// PuTTY-User-Key-File-2: ssh-rsa
	// Encryption: none
	// Comment: rsa-key-20080914
	// Public-Lines: 6
	// AAAAB3NzaC1yc2EAAAADAQABAAABAQC9OMcyDX6cCiBjejxXzNcrB/5W5My0hmCA
	// KJRD01Tzzg94edu17OzHkpG4C02qFa2ASw2DT9oe/qj/sTgckT08KpWlRedNXWZk
	// TXgG+a6wKe7PUcudXcK1JbcE6H3yweLHC7QjjLZFnLSx9pMGfXMNe8K/aatUvkPV
	// Cc5nwH9WLl9x5B15ot7pTOPq9OLkwFaPO9WiD/EOvS0SjgU9Ga7pJSSZPOHASIRN
	// Jybmo6b1WdOvY72wKJsU+aqs4MYDOmP85YgfZEwxPUH02Xo/om8dZlz3auqtRVkc
	// GRKhqQ1aTK0s3mNbLHAh0IXFC8KcAqJzu46h0xKKoNQ8kceU8iCH
	// Private-Lines: 14
	// AAABAAK9V4JPz9beH3dkcqzyfW33VTH6nFX/cr0ISE7c9F+45D4vCTfsfYHvix/m
	// CXbTsJapIagWc9FqZdF2+Xd4ayB7riguwz+x1o9YR4cRywwoLBFP9tzvjMnvkaep
	// AQHTG+HHFUTNskfDVngCwOtlvGHXZPIbU+QLvoGtMFZSOHBrs1cOfoUO1BToN1cG
	// fgp/DuJY6WWVoNs93C8JwgnH2DdosRcMDYOQsASH60hEE2JJuF7Ox+BwlcsV5AUD
	// wIeAChQI0Aend7NL9ceeU8o3UXAVjABowGI9EQJYUmysir5mFs/BETlPambyvEWW
	// NCtJsT2qrCi2Xme5BRzDp5MMJ+EAAACBAM3220GoB4j+PMhomUto+4tVHFf9KYHm
	// prY2sSX9Z48jK5MsTBThVj3yzlS3EZ8eB3KmGL/TqaPJm2JcY1FBcYTAWHcjSAsf
	// DozO3oo0oxorLwIdEttFaC74UNM879f0FHZ4+IzuBVjYB2FBfpq3vJ1/JP7PAHLn
	// QUe5+cPjIT+3AAAAgQDrMLAMYHyKkFAyvIRZU1zR5gUVeiPGUVEqOngFFzjcXSzS
	// DJEtYYJQdM3RnUM8pBGKWjoPycYA03wFhUUMRy4WTxIKSzleTvph8v724ARMq/vL
	// /8ATx3KsJGTz/uTI50yEe2rVsewC61/F6Jit1FbIrRkCkAp1ITL4G91VG/2FsQAA
	// AIEAqHYYHyZI9dah6JVpm5UQ6kJbDPZad7uus/lmJfTgY+ZCH1sEVySLUYk4Tysw
	// yUqzLfazhhQ9yZsDBmdm6jRJLYtQBo9fybpOzeyK20aFIGQTSaVFbAGGYIsv2DOW
	// k9HHr0TSG7yu/1hiMf3ol0ofpTEQpXyM8qjVuqCeqsMeJ9I=
	// Private-MAC: 3BE4CAA1B2AE19C3E6841639BD7275019CF961F1
	const char *toPuttyPrivateKey(bool bEncrypt);

	// Exports the public key to a string in RFC 4716 format. Sample RFC 4716 DSA and
	// RSA public keys are below:
	// 
	// RSA Public Key
	// 
	// ---- BEGIN SSH2 PUBLIC KEY ----
	// Comment: "This is an optional comment"
	// AAAAB3NzaC1yc2EAAAADAQABAAABAQC9OMcyDX6cCiBjejxXzNcrB/5W5My0hmCA
	// KJRD01Tzzg94edu17OzHkpG4C02qFa2ASw2DT9oe/qj/sTgckT08KpWlRedNXWZk
	// TXgG+a6wKe7PUcudXcK1JbcE6H3yweLHC7QjjLZFnLSx9pMGfXMNe8K/aatUvkPV
	// Cc5nwH9WLl9x5B15ot7pTOPq9OLkwFaPO9WiD/EOvS0SjgU9Ga7pJSSZPOHASIRN
	// Jybmo6b1WdOvY72wKJsU+aqs4MYDOmP85YgfZEwxPUH02Xo/om8dZlz3auqtRVkc
	// GRKhqQ1aTK0s3mNbLHAh0IXFC8KcAqJzu46h0xKKoNQ8kceU8iCH
	// ---- END SSH2 PUBLIC KEY ----
	// 
	// DSA Public Key
	// 
	// ---- BEGIN SSH2 PUBLIC KEY ----
	// Comment: "This is an optional comment"
	// AAAAB3NzaC1kc3MAAAEBAKo/+RUI3bL8WrCL8XRCmyHtErh3ycZS0nQqryrJJ6Z9
	// ulAUnVucF6olEpi61O5iyEQFcXUO7pnKdzfMP3SIlZIib5vrVEKpT2K3ep9iSvnk
	// NJRrT3prJ+n0Mx4VJhtIwhYKLd5zaHbKW+XrlMVa1NAQqtj6SkbhirenMquaBAL5
	// YNSuqb6c3vLWfnrWRk6DdrgUP98rBo4Ig7N2YIf5N6PvFLXrgXY3fGsBB3CMxZpf
	// MN/l1hI7jrJVq3WvCojwkWRECGzX4ma3E8wHnzUcyYSiS+hJLglPt6GaPAy+l3Pt
	// Dm5JOuOpGFeaA2GzwQ5jSEAQq+qHTR51pwQZ0rN02y0AAAAVANkiO001oz4lzhsO
	// /tDx9rpOyH/5AAABAA8UN/pP7CYBI1KJ5KvBM7SSd5S5ItjA2ALboHF5uAWBKpaJ
	// vaHyHi/v/eCd1BahglmdTsWoP2W5p4HmHjr6fLseuPGyLTHkGFgKd/zC5eTBid8S
	// hNPJIByKm7XVGvLFhDqhiNKIIsOqYKYkNXmQjms5VInwT4GfE2orVr5MPSg3k3Dt
	// X220CIrEBaPXK4JRdrq2Jezxh7Pp76w+ZEfaQhgf9uEPWtBe0zmKsQ2gjdjRphm+
	// tl4gFR0r4JuJeOTs9UZ4rZlMojK2Ew64rHhaAROHHjOJiQdvBEBYxXNru71sqt7x
	// QbYtqHBR+oBgRFbPPouNMIUexC4DKxyNeuN2zIcAAAEAXh/Akb90+cojwtyjzXTx
	// A9h4CzfTE1G0cWziwToFYPVt/xWZM1/kDEAWRWtTDidZWRxWXxP+8J7PrMwA4Pwo
	// q2SPW1u9qQh1mGpPaPDluPiRbMKL2uV9oLfVEY7naVrqH05EPgtbiNjDin7EQljo
	// 3IoKzpEKB1lFHT/Vd4CMTdl7o+QhZ5ftMGv9sbmf2eZ6y9fQpebO7o4w7/LgQ5Sa
	// IWYaCiTZWNTS4vLeNBuJBVJL/pL3tMQrQFqHAg4o94q6M5Y6NvnsSoyZ3gs5bnuy
	// H/wk2oXdlNhRVx1DMYBSdeWRlgjLYUBEKDABs+1N/9nBIZDEUQYvVA71Fawp4cqi
	// zg==
	// ---- END SSH2 PUBLIC KEY ----
	bool ToRfc4716PublicKey(CkString &outStr);
	// Exports the public key to a string in RFC 4716 format. Sample RFC 4716 DSA and
	// RSA public keys are below:
	// 
	// RSA Public Key
	// 
	// ---- BEGIN SSH2 PUBLIC KEY ----
	// Comment: "This is an optional comment"
	// AAAAB3NzaC1yc2EAAAADAQABAAABAQC9OMcyDX6cCiBjejxXzNcrB/5W5My0hmCA
	// KJRD01Tzzg94edu17OzHkpG4C02qFa2ASw2DT9oe/qj/sTgckT08KpWlRedNXWZk
	// TXgG+a6wKe7PUcudXcK1JbcE6H3yweLHC7QjjLZFnLSx9pMGfXMNe8K/aatUvkPV
	// Cc5nwH9WLl9x5B15ot7pTOPq9OLkwFaPO9WiD/EOvS0SjgU9Ga7pJSSZPOHASIRN
	// Jybmo6b1WdOvY72wKJsU+aqs4MYDOmP85YgfZEwxPUH02Xo/om8dZlz3auqtRVkc
	// GRKhqQ1aTK0s3mNbLHAh0IXFC8KcAqJzu46h0xKKoNQ8kceU8iCH
	// ---- END SSH2 PUBLIC KEY ----
	// 
	// DSA Public Key
	// 
	// ---- BEGIN SSH2 PUBLIC KEY ----
	// Comment: "This is an optional comment"
	// AAAAB3NzaC1kc3MAAAEBAKo/+RUI3bL8WrCL8XRCmyHtErh3ycZS0nQqryrJJ6Z9
	// ulAUnVucF6olEpi61O5iyEQFcXUO7pnKdzfMP3SIlZIib5vrVEKpT2K3ep9iSvnk
	// NJRrT3prJ+n0Mx4VJhtIwhYKLd5zaHbKW+XrlMVa1NAQqtj6SkbhirenMquaBAL5
	// YNSuqb6c3vLWfnrWRk6DdrgUP98rBo4Ig7N2YIf5N6PvFLXrgXY3fGsBB3CMxZpf
	// MN/l1hI7jrJVq3WvCojwkWRECGzX4ma3E8wHnzUcyYSiS+hJLglPt6GaPAy+l3Pt
	// Dm5JOuOpGFeaA2GzwQ5jSEAQq+qHTR51pwQZ0rN02y0AAAAVANkiO001oz4lzhsO
	// /tDx9rpOyH/5AAABAA8UN/pP7CYBI1KJ5KvBM7SSd5S5ItjA2ALboHF5uAWBKpaJ
	// vaHyHi/v/eCd1BahglmdTsWoP2W5p4HmHjr6fLseuPGyLTHkGFgKd/zC5eTBid8S
	// hNPJIByKm7XVGvLFhDqhiNKIIsOqYKYkNXmQjms5VInwT4GfE2orVr5MPSg3k3Dt
	// X220CIrEBaPXK4JRdrq2Jezxh7Pp76w+ZEfaQhgf9uEPWtBe0zmKsQ2gjdjRphm+
	// tl4gFR0r4JuJeOTs9UZ4rZlMojK2Ew64rHhaAROHHjOJiQdvBEBYxXNru71sqt7x
	// QbYtqHBR+oBgRFbPPouNMIUexC4DKxyNeuN2zIcAAAEAXh/Akb90+cojwtyjzXTx
	// A9h4CzfTE1G0cWziwToFYPVt/xWZM1/kDEAWRWtTDidZWRxWXxP+8J7PrMwA4Pwo
	// q2SPW1u9qQh1mGpPaPDluPiRbMKL2uV9oLfVEY7naVrqH05EPgtbiNjDin7EQljo
	// 3IoKzpEKB1lFHT/Vd4CMTdl7o+QhZ5ftMGv9sbmf2eZ6y9fQpebO7o4w7/LgQ5Sa
	// IWYaCiTZWNTS4vLeNBuJBVJL/pL3tMQrQFqHAg4o94q6M5Y6NvnsSoyZ3gs5bnuy
	// H/wk2oXdlNhRVx1DMYBSdeWRlgjLYUBEKDABs+1N/9nBIZDEUQYvVA71Fawp4cqi
	// zg==
	// ---- END SSH2 PUBLIC KEY ----
	const char *toRfc4716PublicKey(void);

	// Exports an SSH key to XML. Samples of RSA and DSA private keys are shown below:
	// 
	// DSA Key
	// _LT_DSAKeyValue>
	//     _LT_P>qj/5FQjdsvxasIvxd......Ss3TbLQ==_LT_/P>
	//     _LT_Q>2SI7TTWjPiXOGw7+0PH2uk7If/k=_LT_/Q>
	//     _LT_G>DxQ3+k/sJgEjUon......643bMhw==_LT_/G>
	//     _LT_Y>Xh/Akb90+cojwty.....VA71p4cqizg==_LT_/Y>
	//     _LT_X>ojilJuWXOyBTQ0H4hBNBTsL9Mq8=_LT_/X>
	// _LT_/DSAKeyValue>
	// 
	// RSA Key
	// _LT_RSAKeyValue>
	//     _LT_Modulus>vTjHMg1+n.....JHHlPIghw==_LT_/Modulus>
	//     _LT_Exponent>AQAB_LT_/Exponent>
	//     _LT_P>zfbbQagHiP48y.....udBR7n5w+MhP7c=_LT_/P>
	//     _LT_Q>6zCwDGB8ipB....dSEy+BvdVRv9hbE=_LT_/Q>
	//     _LT_DP>zcNdvlUg2g......9F72NjnAhQUjc=_LT_/DP>
	//     _LT_DQ>1XB2FIVsA.....qNewUutJKWXcE=_LT_/DQ>
	//     _LT_InverseQ>qHYYHyZI9d....VuqCeqsMeJ9I=_LT_/InverseQ>
	//     _LT_D>Ar1Xgk/P1t4fd2Ryr........Onkwwn4Q==_LT_/D>
	// _LT_/RSAKeyValue>
	// 
	bool ToXml(CkString &outStr);
	// Exports an SSH key to XML. Samples of RSA and DSA private keys are shown below:
	// 
	// DSA Key
	// _LT_DSAKeyValue>
	//     _LT_P>qj/5FQjdsvxasIvxd......Ss3TbLQ==_LT_/P>
	//     _LT_Q>2SI7TTWjPiXOGw7+0PH2uk7If/k=_LT_/Q>
	//     _LT_G>DxQ3+k/sJgEjUon......643bMhw==_LT_/G>
	//     _LT_Y>Xh/Akb90+cojwty.....VA71p4cqizg==_LT_/Y>
	//     _LT_X>ojilJuWXOyBTQ0H4hBNBTsL9Mq8=_LT_/X>
	// _LT_/DSAKeyValue>
	// 
	// RSA Key
	// _LT_RSAKeyValue>
	//     _LT_Modulus>vTjHMg1+n.....JHHlPIghw==_LT_/Modulus>
	//     _LT_Exponent>AQAB_LT_/Exponent>
	//     _LT_P>zfbbQagHiP48y.....udBR7n5w+MhP7c=_LT_/P>
	//     _LT_Q>6zCwDGB8ipB....dSEy+BvdVRv9hbE=_LT_/Q>
	//     _LT_DP>zcNdvlUg2g......9F72NjnAhQUjc=_LT_/DP>
	//     _LT_DQ>1XB2FIVsA.....qNewUutJKWXcE=_LT_/DQ>
	//     _LT_InverseQ>qHYYHyZI9d....VuqCeqsMeJ9I=_LT_/InverseQ>
	//     _LT_D>Ar1Xgk/P1t4fd2Ryr........Onkwwn4Q==_LT_/D>
	// _LT_/RSAKeyValue>
	// 
	const char *toXml(void);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
