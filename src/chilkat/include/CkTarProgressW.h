// CkTarProgress.h: interface for the CkTarProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKTARPROGRESSW_H
#define _CKTARPROGRESSW_H

#include "CkBaseProgressW.h"

#include "ck_inttypes.h"

// When creating an application class that inherits the CkTarProgressW base class, use the CK_TARPROGRESSW_API 
// definition to declare the overrides in the class header.  This has the effect that if for
// some unforeseen and unlikely reason the Chilkat event callback API changes, or if new
// callback methods are added in a future version, then you'll discover them at compile time
// after updating to the new Chilkat version.  
// For example:
/*
    class MyProgress : public CkTarProgressW
    {
	public:
	    CK_TARPROGRESSW_API

	...
    };
*/
#define CK_TARPROGRESSW_API \
	void NextTarFile(const wchar_t *path, __int64 fileSize,bool bIsDirectory, bool *skip);


#ifndef __sun__
#pragma pack (push, 8)
#endif


class CkTarProgressW : public CkBaseProgressW 
{
    public:
	CkTarProgressW() { }
	virtual ~CkTarProgressW() { }


	// These event callbacks are now defined in CkBaseProgressW.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const wchar_t *name, const wchar_t *value) { }

	// Called just before appending to TAR when writing a .tar, and just before
	// extracting during untar.
	virtual void NextTarFile(const wchar_t *path, 
	    __int64 fileSize,
	    bool bIsDirectory, 
	    bool *skip) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
