// CkFtpProgress.h: interface for the CkFtpProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKFTP2PROGRESSW_H
#define _CKFTP2PROGRESSW_H


#include "CkBaseProgressW.h"

/*

  To receive progress events (callbacks), create a C++ class that 
  inherits this one and provides one or more overriding implementations 
  for the events you wish to receive.

  */

// When creating an application class that inherits the CkFtp2ProgressW base class, use the CK_FTP2PROGRESSW_API 
// definition to declare the overrides in the class header.  This has the effect that if for
// some unforeseen and unlikely reason the Chilkat event callback API changes, or if new
// callback methods are added in a future version, then you'll discover them at compile time
// after updating to the new Chilkat version.  
// For example:
/*
    class MyProgress : public CkFtp2ProgressW
    {
	public:
	    CK_FTP2PROGRESSW_API

	...
    };
*/
#define CK_FTP2PROGRESSW_API \
	void BeginDownloadFile(const wchar_t *pathUtf8, bool *skip);\
	void EndDownloadFile(const wchar_t *pathUtf8, __int64 numBytes);\
	void VerifyDownloadDir(const wchar_t *pathUtf8, bool *skip);\
	void BeginUploadFile(const wchar_t *pathUtf8, bool *skip);\
	void EndUploadFile(const wchar_t *pathUtf8, __int64 numBytes);\
	void VerifyUploadDir(const wchar_t *pathUtf8, bool *skip);\
	void VerifyDeleteDir(const wchar_t *pathUtf8, bool *skip);\
	void VerifyDeleteFile(const wchar_t *pathUtf8, bool *skip);\
	void UploadRate(__int64 byteCount, unsigned long bytesPerSec);\
	void DownloadRate(__int64 byteCount, unsigned long bytesPerSec);


#ifndef __sun__
#pragma pack (push, 8)
#endif

class CkFtp2ProgressW : public CkBaseProgressW
{
    public:

	CkFtp2ProgressW() { }
	virtual ~CkFtp2ProgressW() { }

	// These event callbacks are now defined in CkBaseProgressW.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const wchar_t *name, const wchar_t *value) { }


	virtual void BeginDownloadFile(const wchar_t *pathUtf8, bool *skip) { }
	virtual void EndDownloadFile(const wchar_t *pathUtf8, __int64 numBytes) { }
	virtual void VerifyDownloadDir(const wchar_t *pathUtf8, bool *skip) { }

	virtual void BeginUploadFile(const wchar_t *pathUtf8, bool *skip) { }
	virtual void EndUploadFile(const wchar_t *pathUtf8, __int64 numBytes) { }
	virtual void VerifyUploadDir(const wchar_t *pathUtf8, bool *skip) { }

	virtual void VerifyDeleteDir(const wchar_t *pathUtf8, bool *skip) { }
	virtual void VerifyDeleteFile(const wchar_t *pathUtf8, bool *skip) { }

	virtual void UploadRate(__int64 byteCount, unsigned long bytesPerSec) { }
	virtual void DownloadRate(__int64 byteCount, unsigned long bytesPerSec) { }



};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
