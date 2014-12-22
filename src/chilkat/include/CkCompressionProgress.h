// CkCrypt2Progress.h: interface for the CkCrypt2Progress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKCOMPRESSIONPROGRESS_H
#define _CKCOMPRESSIONPROGRESS_H

#include "CkBaseProgress.h"


#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkCompressionProgress : public CkBaseProgress  
{
    public:
	CkCompressionProgress() { }
	virtual ~CkCompressionProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
