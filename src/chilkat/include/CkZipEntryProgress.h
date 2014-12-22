// CkZipEntryProgress.h: interface for the CkZipEntryProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKZIPENTRYPROGRESS_H
#define _CKZIPENTRYPROGRESS_H

#include "CkBaseProgress.h"

#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkZipEntryProgress : public CkBaseProgress 
{
    public:
	CkZipEntryProgress() { }
	virtual ~CkZipEntryProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
