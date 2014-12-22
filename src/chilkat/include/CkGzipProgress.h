// CkGzipProgress.h: interface for the CkGzipProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKGZIPPROGRESS_H
#define _CKGZIPPROGRESS_H

#include "CkBaseProgress.h"


#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkGzipProgress  : public CkBaseProgress 
{
    public:
	CkGzipProgress() { }
	virtual ~CkGzipProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
