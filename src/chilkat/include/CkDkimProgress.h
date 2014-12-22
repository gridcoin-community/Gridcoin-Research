// CkDkimProgress.h: interface for the CkDkimProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKDKIMPROGRESS_H
#define _CKDKIMPROGRESS_H

#include "CkBaseProgress.h"


#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkDkimProgress : public CkBaseProgress  
{
    public:
	CkDkimProgress() { }
	virtual ~CkDkimProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
