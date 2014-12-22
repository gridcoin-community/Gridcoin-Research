// CkImapProgress.h: interface for the CkImapProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKIMAPPROGRESS_H
#define _CKIMAPPROGRESS_H

#include "CkBaseProgress.h"


#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkImapProgress : public CkBaseProgress 
{
    public:
	CkImapProgress() { }
	virtual ~CkImapProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
