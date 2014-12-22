// CkMhtProgress.h: interface for the CkMhtProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKMHTPROGRESS_H
#define _CKMHTPROGRESS_H

#include "CkBaseProgress.h"


#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkMhtProgress  : public CkBaseProgress 
{
    public:
	CkMhtProgress() { }
	virtual ~CkMhtProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
