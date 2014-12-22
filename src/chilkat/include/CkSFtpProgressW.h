// CkSFtpProgress.h: interface for the CkSFtpProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKSFTPPROGRESSW_H
#define _CKSFTPPROGRESSW_H

#include "CkBaseProgressW.h"

// When creating an application class that inherits the CkSFtpProgressW base class, use the CK_SFTPPROGRESSW_API 
// definition to declare the overrides in the class header.  This has the effect that if for
// some unforeseen and unlikely reason the Chilkat event callback API changes, or if new
// callback methods are added in a future version, then you'll discover them at compile time
// after updating to the new Chilkat version.  
// For example:
/*
    class MyProgress : public CkSFtpProgressW
    {
	public:
	    CK_SFTPPROGRESSW_API

	...
    };
*/
#define CK_SFTPPROGRESSW_API \
	void UploadRate(__int64 byteCount, unsigned long bytesPerSec);\
	void DownloadRate(__int64 byteCount, unsigned long bytesPerSec);


#ifndef __sun__
#pragma pack (push, 8)
#endif

class CkSFtpProgressW : public CkBaseProgressW 
{
    public:
	CkSFtpProgressW() { }
	virtual ~CkSFtpProgressW() { }

	// These event callbacks are now defined in CkBaseProgressW.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const wchar_t *name, const wchar_t *value) { }

	virtual void UploadRate(__int64 byteCount, unsigned long bytesPerSec) { }
	virtual void DownloadRate(__int64 byteCount, unsigned long bytesPerSec) { }


};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
