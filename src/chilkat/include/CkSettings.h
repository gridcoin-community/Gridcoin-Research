// This source file is not generated.

#if !defined(_CKSETTINGS_H_INCLUDED_)
#define _CKSETTINGS_H_INCLUDED_

#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkSettings  
{
public:
	CkSettings();
	virtual ~CkSettings();

	// CLASS: CkSettings

	// Applies to Windows systems only.  Indicates whether the Windows CA cert stores (registry based, such as Current User or Local Machine)
	// are automatically consulted to implicitly trust root CA certificates. (Meaning that if a certificate is found in 
	// a CA cert store, then it is assumed to be trusted.)
	// The default value is true.
	static bool m_trustSystemCaRoots;

	// Each Chilkat object's Utf8 property will default to the value of 
	// CkSettings::m_utf8.  The default is false (meaning ANSI).
	static bool m_utf8;  

	// If set to true, then all LastErrorText's will be verbose w.r.t. SSL/TLS internals.
	// The default is false.
	static bool m_verboseSsl;

	// If set to true, then all LastErrorText's will be verbose w.r.t. MIME parsing internals.
	// The default is false.
	static bool m_verboseMime;


	// BEGIN PUBLIC INTERFACE

	// Call this once at the beginning of your program
	// if your program is multithreaded.
	static void initializeMultithreaded(void);

	// Call this once at the end of your program.
	// It is only necessary if you are testing for memory leaks.
	static void cleanupMemory(void);


	// Get the sum of the sizes of all the process heaps.
	static unsigned long getTotalSizeProcessHeaps(void);


	static int getAnsiCodePage(void);
	static int getOemCodePage(void);
	static void setAnsiCodePage(int cp);
	static void setOemCodePage(int cp);

	// END PUBLIC INTERFACE
};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif // !defined(_CKSETTINGS_H_INCLUDED_)
