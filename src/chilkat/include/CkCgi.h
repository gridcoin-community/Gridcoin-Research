// CkCgi.h: interface for the CkCgi class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkCgi_H
#define _CkCgi_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkCgi
class CK_VISIBLE_PUBLIC CkCgi  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkCgi(const CkCgi &);
	CkCgi &operator=(const CkCgi &);

    public:
	CkCgi(void);
	virtual ~CkCgi(void);

	static CkCgi *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------

	unsigned long get_AsyncBytesRead(void);


	bool get_AsyncInProgress(void);


	unsigned long get_AsyncPostSize(void);


	bool get_AsyncSuccess(void);


	int get_HeartbeatMs(void);

	void put_HeartbeatMs(int newVal);


	int get_IdleTimeoutMs(void);

	void put_IdleTimeoutMs(int newVal);


	int get_NumParams(void);


	int get_NumUploadFiles(void);


	int get_ReadChunkSize(void);

	void put_ReadChunkSize(int newVal);


	unsigned long get_SizeLimitKB(void);

	void put_SizeLimitKB(unsigned long newVal);


	bool get_StreamToUploadDir(void);

	void put_StreamToUploadDir(bool newVal);


	void get_UploadDir(CkString &str);

	const char *uploadDir(void);

	void put_UploadDir(const char *newVal);



	// ----------------------
	// Methods
	// ----------------------

	void AbortAsync(void);

#if defined(WIN32) && !defined(SINGLE_THREADED)

	bool AsyncReadRequest(void);
#endif


	bool GetEnv(const char *varName, CkString &outStr);

	const char *getEnv(const char *varName);

	const char *env(const char *varName);


	bool GetParam(const char *paramName, CkString &outStr);

	const char *getParam(const char *paramName);

	const char *param(const char *paramName);


	bool GetParamName(int index, CkString &outStr);

	const char *getParamName(int index);

	const char *paramName(int index);


	bool GetParamValue(int index, CkString &outStr);

	const char *getParamValue(int index);

	const char *paramValue(int index);


	bool GetRawPostData(CkByteData &outData);


	bool GetUploadData(int index, CkByteData &outData);


	bool GetUploadFilename(int index, CkString &outStr);

	const char *getUploadFilename(int index);

	const char *uploadFilename(int index);


	unsigned long GetUploadSize(int index);


	bool IsGet(void);


	bool IsHead(void);


	bool IsPost(void);


	bool IsUpload(void);


	bool ReadRequest(void);


	bool SaveNthToUploadDir(int index);


	void SleepMs(int millisec);


	bool TestConsumeAspUpload(const char *path);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
