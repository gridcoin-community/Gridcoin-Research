// CkBz2W.h: interface for the CkBz2W class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkBz2W_H
#define _CkBz2W_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkByteData;
class CkBaseProgressW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkBz2W
class CK_VISIBLE_PUBLIC CkBz2W  : public CkWideCharBase
{
    private:
	bool m_cbOwned;
	CkBaseProgressW *m_callback;

	// Don't allow assignment or copying these objects.
	CkBz2W(const CkBz2W &);
	CkBz2W &operator=(const CkBz2W &);

    public:
	CkBz2W(void);
	virtual ~CkBz2W(void);

	static CkBz2W *createNew(void);
	

	CkBz2W(bool bCallbackOwned);
	static CkBz2W *createNew(bool bCallbackOwned);

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	CkBaseProgressW *get_EventCallbackObject(void) const;
	void put_EventCallbackObject(CkBaseProgressW *progress);


	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------

	int get_HeartbeatMs(void);

	void put_HeartbeatMs(int newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Compresses a file to create a BZip2 compressed file (.bz2).
	bool CompressFile(const wchar_t *inFilename, const wchar_t *toPath);

	// BZip2 compresses a file to an in-memory image of a .bz2 file.
	bool CompressFileToMem(const wchar_t *inFilename, CkByteData &outBytes);

	// BZip2 compresses and creates a .bz2 file from in-memory data.
	bool CompressMemToFile(const CkByteData &inData, const wchar_t *toPath);

	// Compresses in-memory data to an in-memory image of a .bz2 file.
	bool CompressMemory(const CkByteData &inData, CkByteData &outBytes);

	// Unzips a .bz2 file.
	bool UncompressFile(const wchar_t *inFilename, const wchar_t *toPath);

	// Unzips a .bz2 file directly to memory.
	bool UncompressFileToMem(const wchar_t *inFilename, CkByteData &outBytes);

	// Unzips from an in-memory image of a .bz2 file to a file.
	bool UncompressMemToFile(const CkByteData &inData, const wchar_t *toPath);

	// Unzips from an in-memory image of a .bz2 file directly into memory.
	bool UncompressMemory(const CkByteData &inData, CkByteData &outBytes);

	// Unlocks the component allowing for the full functionality to be used. If a
	// permanent (purchased) unlock code is passed, there is no expiration. Any other
	// string automatically begins a fully-functional 30-day trial the first time
	// UnlockComponent is called.
	bool UnlockComponent(const wchar_t *regCode);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
