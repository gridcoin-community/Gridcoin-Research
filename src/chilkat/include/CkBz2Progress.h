// CkBz2Progress.h: interface for the CkBz2Progress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CkBz2PROGRESS_H
#define _CkBz2PROGRESS_H

#include "CkBaseProgress.h"


#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkBz2Progress : public CkBaseProgress  
{
    public:
	CkBz2Progress() { }
	virtual ~CkBz2Progress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
