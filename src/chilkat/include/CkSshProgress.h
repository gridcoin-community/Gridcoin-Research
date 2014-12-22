// CkSshProgress.h: interface for the CkSshProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKSshPROGRESS_H
#define _CKSshPROGRESS_H

#include "CkBaseProgress.h"

#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkSshProgress : public CkBaseProgress 
{
    public:
	CkSshProgress() { }
	virtual ~CkSshProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
