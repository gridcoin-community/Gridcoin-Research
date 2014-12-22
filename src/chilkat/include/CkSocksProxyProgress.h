// CkSocksProxyProgress.h: interface for the CkSocksProxyProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKSOCKSPROXYPROGRESS_H
#define _CKSOCKSPROXYPROGRESS_H

#include "CkBaseProgress.h"


#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkSocksProxyProgress : public CkBaseProgress
{
    public:
	CkSocksProxyProgress() { }
	virtual ~CkSocksProxyProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
