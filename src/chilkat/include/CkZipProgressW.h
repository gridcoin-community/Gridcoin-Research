// CkZipProgress.h: interface for the CkZipProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKZIPPROGRESSW_H
#define _CKZIPPROGRESSW_H

#include "CkBaseProgressW.h"

/*

  To receive progress events (callbacks), create a C++ class that 
  inherits this one and provides one or more overriding implementations 
  for the events you wish to receive. 

  */

// When creating an application class that inherits the CkZipProgressW base class, use the CK_ZIPPROGRESSW_API 
// definition to declare the overrides in the class header.  This has the effect that if for
// some unforeseen and unlikely reason the Chilkat event callback API changes, or if new
// callback methods are added in a future version, then you'll discover them at compile time
// after updating to the new Chilkat version.  
// For example:
/*
    class MyProgress : public CkZipProgressW
    {
	public:
	    CK_ZIPPROGRESSW_API

	...
    };
*/
#define CK_ZIPPROGRESSW_API \
	void ToBeAdded(const wchar_t *filePath, __int64 fileSize, bool *skip);\
	void DirToBeAdded(const wchar_t *filePath, bool *skip);\
	void FileAdded(const wchar_t *filePath, __int64 fileSize, bool *abort);\
	void ToBeZipped(const wchar_t *filePath, __int64 fileSize, bool *skip);\
	void FileZipped(const wchar_t *filePath, __int64 fileSize, __int64 compressedSize, bool *abort);\
	void ToBeUnzipped(const wchar_t *filePath, __int64 compressedSize,__int64 fileSize, bool isDirectory,bool *skip);\
	void FileUnzipped(const wchar_t *filePath, __int64 compressedSize,__int64 fileSize, bool isDirectory,bool *abort);\
	void SkippedForUnzip(const wchar_t *filePath, __int64 compressedSize,__int64 fileSize,bool isDirectory);\
	void AddFilesBegin(void);\
	void AddFilesEnd(void);\
	void WriteZipBegin(void);\
	void WriteZipEnd(void);\
	void UnzipBegin(void);\
	void UnzipEnd(void);


#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CkZipProgressW : public CkBaseProgressW 
{
    public:
	CkZipProgressW() { }
	virtual ~CkZipProgressW() { }

	// These event callbacks are now defined in CkBaseProgressW.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const wchar_t *name, const wchar_t *value) { }

	virtual void ToBeAdded(const wchar_t *filePath, 
	    __int64 fileSize, 
	    bool *skip) { }

	virtual void DirToBeAdded(const wchar_t *filePath, 
	    bool *skip) { }

	virtual void FileAdded(const wchar_t *filePath, 
	    __int64 fileSize, 
	    bool *abort) { }

	virtual void ToBeZipped(const wchar_t *filePath, 
	    __int64 fileSize, 
	    bool *skip) { }

	virtual void FileZipped(const wchar_t *filePath, 
	    __int64 fileSize, 
	    __int64 compressedSize, 
	    bool *abort) { }

	virtual void ToBeUnzipped(const wchar_t *filePath, 
	    __int64 compressedSize,
	    __int64 fileSize, 
	    bool isDirectory,
	    bool *skip) { }

	virtual void FileUnzipped(const wchar_t *filePath, 
	    __int64 compressedSize,
	    __int64 fileSize, 
	    bool isDirectory,
	    bool *abort) { }

	virtual void SkippedForUnzip(const wchar_t *filePath, 
	    __int64 compressedSize,
	    __int64 fileSize,
	    bool isDirectory) { }

	virtual void AddFilesBegin(void) { }
	virtual void AddFilesEnd(void) { }
	virtual void WriteZipBegin(void) { }
	virtual void WriteZipEnd(void) { }
	virtual void UnzipBegin(void) { }
	virtual void UnzipEnd(void) { }
};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
