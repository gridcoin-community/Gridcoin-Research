// CkAtomProgress.h: interface for the CkAtomProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKAtomPROGRESS_H
#define _CKAtomPROGRESS_H

#include "CkBaseProgress.h"

#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkAtomProgress : public CkBaseProgress
{
    public:
	CkAtomProgress() { }
	virtual ~CkAtomProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
