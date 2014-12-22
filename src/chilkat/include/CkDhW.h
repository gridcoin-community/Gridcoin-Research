// CkDhW.h: interface for the CkDhW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkDhW_H
#define _CkDhW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"




#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkDhW
class CK_VISIBLE_PUBLIC CkDhW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkDhW(const CkDhW &);
	CkDhW &operator=(const CkDhW &);

    public:
	CkDhW(void);
	virtual ~CkDhW(void);

	static CkDhW *createNew(void);
	

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// A "safe" large prime returned as a hex string. The hex string represent a bignum
	// in SSH1 format.
	void get_P(CkString &str);
	// A "safe" large prime returned as a hex string. The hex string represent a bignum
	// in SSH1 format.
	const wchar_t *p(void);

	// The generator. The value of G should be either 2 or 5.
	int get_G(void);



	// ----------------------
	// Methods
	// ----------------------
	// The 2nd and final step in Diffie-Hellman (DH) key exchange. E is the E
	// created by the other party. Returns the shared secret (K) as an SSH1-format
	// bignum encoded as a hex string.
	bool FindK(const wchar_t *e, CkString &outStr);
	// The 2nd and final step in Diffie-Hellman (DH) key exchange. E is the E
	// created by the other party. Returns the shared secret (K) as an SSH1-format
	// bignum encoded as a hex string.
	const wchar_t *findK(const wchar_t *e);

	// Generates a large safe prime that is numBits bits in size using the generator  G.
	// Generating a new (random) P is expensive in both time and CPU cycles. A prime
	// should be 1024 or more bits in length.
	bool GenPG(int numBits, int g);

	// Sets explicit values for P and G. Returns true if P and G conform to the
	// requirements for Diffie-Hellman. P is an SSH1-format bignum passed as a
	// hexidecimalized string.
	bool SetPG(const wchar_t *p, int g);

	// Unlocks the component. This must be called once prior to calling any other
	// method. A fully-functional 30-day trial is automatically started when an
	// arbitrary string is passed to this method. For example, passing "Hello", or
	// "abc123" will unlock the component for the 1st thirty days after the initial
	// install.
	bool UnlockComponent(const wchar_t *unlockCode);

	// Sets P and G to a known safe prime. The index may have the following values:
	// 
	// 1: First Oakley Default Group from RFC2409, section 6.1. Generator is 2. The
	// prime is: 2^768 - 2 ^704 - 1 + 2^64 * { [2^638 pi] + 149686 }
	// 
	// 2: Prime for 2nd Oakley Group (RFC 2409) -- 1024-bit MODP Group. Generator is 2.
	// The prime is: 2^1024 - 2^960 - 1 + 2^64 * { [2^894 pi] + 129093 }.
	// 
	// 3: 1536-bit MODP Group from RFC3526, Section 2. Generator is 2. The prime is:
	// 2^1536 - 2^1472 - 1 + 2^64 * { [2^1406 pi] + 741804 }
	// 
	// 4: Prime for 14th Oakley Group (RFC 3526) -- 2048-bit MODP Group. Generator is
	// 2. The prime is: 2^2048 - 2^1984 - 1 + 2^64 * { [2^1918 pi] + 124476 }
	// 
	// 5: 3072-bit MODP Group from RFC3526, Section 4. Generator is 2. The prime is:
	// 2^3072 - 2^3008 - 1 + 2^64 * { [2^2942 pi] + 1690314 }
	// 
	// 6: 4096-bit MODP Group from RFC3526, Section 5. Generator is 2. The prime is:
	// 2^4096 - 2^4032 - 1 + 2^64 * { [2^3966 pi] + 240904 }
	// 
	// 7: 6144-bit MODP Group from RFC3526, Section 6. Generator is 2. The prime is:
	// 2^6144 - 2^6080 - 1 + 2^64 * { [2^6014 pi] + 929484 }
	// 
	// 8: 8192-bit MODP Group from RFC3526, Section 7. Generator is 2. The prime is:
	// 2^8192 - 2^8128 - 1 + 2^64 * { [2^8062 pi] + 4743158 }
	// 
	void UseKnownPrime(int index);

	// The 1st step in Diffie-Hellman key exchange (to generate a shared-secret). The
	// numBits should be twice the size (in bits) of the shared secret to be generated.
	// For example, if you are using DH to create a 128-bit AES session key, then numBits
	// should be set to 256. Returns E as a bignum in SSH-format as a hex string.
	bool CreateE(int numBits, CkString &outStr);
	// The 1st step in Diffie-Hellman key exchange (to generate a shared-secret). The
	// numBits should be twice the size (in bits) of the shared secret to be generated.
	// For example, if you are using DH to create a 128-bit AES session key, then numBits
	// should be set to 256. Returns E as a bignum in SSH-format as a hex string.
	const wchar_t *createE(int numBits);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
