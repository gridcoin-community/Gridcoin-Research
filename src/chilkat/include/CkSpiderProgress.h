// CkSpiderProgress.h: interface for the CkSpiderProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKSPIDERPROGRESS_H
#define _CKSPIDERPROGRESS_H

#include "CkBaseProgress.h"


class CkSpiderProgress : public CkBaseProgress  
{
    public:
	CkSpiderProgress() { }
	virtual ~CkSpiderProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};

#endif
