// CkTarProgress.h: interface for the CkTarProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKTARPROGRESS_H
#define _CKTARPROGRESS_H

#include "CkBaseProgress.h"

// When creating an application class that inherits the CkTarProgress base class, use the CK_TARPROGRESS_API 
// definition to declare the overrides in the class header.  This has the effect that if for
// some unforeseen and unlikely reason the Chilkat event callback API changes, or if new
// callback methods are added in a future version, then you'll discover them at compile time
// after updating to the new Chilkat version.  
// For example:
/*
    class MyProgress : public CkTarProgress
    {
	public:
	    CK_TARPROGRESS_API

	...
    };
*/
#define CK_TARPROGRESS_API \
	void NextTarFile(const char *path, __int64 fileSize,bool bIsDirectory, bool *skip);


#ifndef __sun__
#pragma pack (push, 8)
#endif
 
class CkTarProgress : public CkBaseProgress 
{
    public:
	CkTarProgress() { }
	virtual ~CkTarProgress() { }


	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

	// Called just before appending to TAR when writing a .tar, and just before
	// extracting during untar.
	virtual void NextTarFile(const char *path, 
	    __int64 fileSize,
	    bool bIsDirectory, 
	    bool *skip) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
