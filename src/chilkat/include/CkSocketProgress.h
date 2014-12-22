// CkSocketProgress.h: interface for the CkSocketProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKSOCKETPROGRESS_H
#define _CKSOCKETPROGRESS_H

#include "CkBaseProgress.h"


class CkString;

#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkSocketProgress : public CkBaseProgress 
{
    public:
	CkSocketProgress() { }
	virtual ~CkSocketProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
