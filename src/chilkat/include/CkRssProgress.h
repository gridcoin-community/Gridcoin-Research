// CkRssProgress.h: interface for the CkRssProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKRssPROGRESS_H
#define _CKRssPROGRESS_H

#include "CkBaseProgress.h"


#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkRssProgress : public CkBaseProgress  
{
    public:
	CkRssProgress() { }
	virtual ~CkRssProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
