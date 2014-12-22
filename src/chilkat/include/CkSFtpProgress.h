// CkSFtpProgress.h: interface for the CkSFtpProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKSFTPPROGRESS_H
#define _CKSFTPPROGRESS_H

#include "CkBaseProgress.h"

// When creating an application class that inherits the CkSFtpProgress base class, use the CK_SFTPPROGRESS_API 
// definition to declare the overrides in the class header.  This has the effect that if for
// some unforeseen and unlikely reason the Chilkat event callback API changes, or if new
// callback methods are added in a future version, then you'll discover them at compile time
// after updating to the new Chilkat version.  
// For example:
/*
    class MyProgress : public CkSFtpProgress
    {
	public:
	    CK_SFTPPROGRESS_API

	...
    };
*/
#define CK_SFTPPROGRESS_API \
	void UploadRate(__int64 byteCount, unsigned long bytesPerSec);\
	void DownloadRate(__int64 byteCount, unsigned long bytesPerSec);


#ifndef __sun__
#pragma pack (push, 8)
#endif
 
class CkSFtpProgress : public CkBaseProgress 
{
    public:
	CkSFtpProgress() { }
	virtual ~CkSFtpProgress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

	virtual void UploadRate(__int64 byteCount, unsigned long bytesPerSec) { }
	virtual void DownloadRate(__int64 byteCount, unsigned long bytesPerSec) { }


};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
