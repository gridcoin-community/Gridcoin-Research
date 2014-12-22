// CkFtpProgress.h: interface for the CkFtpProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKFTP2PROGRESS_H
#define _CKFTP2PROGRESS_H


#include "CkBaseProgress.h"

/*

  To receive progress events (callbacks), create a C++ class that 
  inherits this one and provides one or more overriding implementations 
  for the events you wish to receive.

  */


// When creating an application class that inherits the CkFtp2Progress base class, use the CK_FTP2PROGRESS_API 
// definition to declare the overrides in the class header.  This has the effect that if for
// some unforeseen and unlikely reason the Chilkat event callback API changes, or if new
// callback methods are added in a future version, then you'll discover them at compile time
// after updating to the new Chilkat version.  
// For example:
/*
    class MyProgress : public CkFtp2Progress
    {
	public:
	    CK_FTP2PROGRESS_API

	...
    };
*/
#define CK_FTP2PROGRESS_API \
	void BeginDownloadFile(const char *pathUtf8, bool *skip);\
	void EndDownloadFile(const char *pathUtf8, __int64 numBytes);\
	void VerifyDownloadDir(const char *pathUtf8, bool *skip);\
	void BeginUploadFile(const char *pathUtf8, bool *skip);\
	void EndUploadFile(const char *pathUtf8, __int64 numBytes);\
	void VerifyUploadDir(const char *pathUtf8, bool *skip);\
	void VerifyDeleteDir(const char *pathUtf8, bool *skip);\
	void VerifyDeleteFile(const char *pathUtf8, bool *skip);\
	void UploadRate(__int64 byteCount, unsigned long bytesPerSec);\
	void DownloadRate(__int64 byteCount, unsigned long bytesPerSec);



#ifndef __sun__
#pragma pack (push, 8)
#endif
 
class CkFtp2Progress : public CkBaseProgress
{
    public:

	CkFtp2Progress() { }
	virtual ~CkFtp2Progress() { }

	// These event callbacks are now defined in CkBaseProgress.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const char *name, const char *value) { }

	virtual void BeginDownloadFile(const char *pathUtf8, bool *skip) { }
	virtual void EndDownloadFile(const char *pathUtf8, __int64 numBytes) { }
	virtual void VerifyDownloadDir(const char *pathUtf8, bool *skip) { }

	virtual void BeginUploadFile(const char *pathUtf8, bool *skip) { }
	virtual void EndUploadFile(const char *pathUtf8, __int64 numBytes) { }
	virtual void VerifyUploadDir(const char *pathUtf8, bool *skip) { }

	virtual void VerifyDeleteDir(const char *pathUtf8, bool *skip) { }
	virtual void VerifyDeleteFile(const char *pathUtf8, bool *skip) { }

	virtual void UploadRate(__int64 byteCount, unsigned long bytesPerSec) { }
	virtual void DownloadRate(__int64 byteCount, unsigned long bytesPerSec) { }



};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
