#ifndef _CkWideCharBase_H
#define _CkWideCharBase_H
#pragma once

#ifndef __sun__
#pragma pack (push, 8)
#endif

#include "CkObject.h"
#include "CkString.h"

class CK_VISIBLE_PUBLIC CkWideCharBase : public  CkObject
{
    private:
		
	// Disallow assignment or copying this object.
	CkWideCharBase(const CkWideCharBase &);
	CkWideCharBase &operator=(const CkWideCharBase &);

    protected:
	void *m_impl;	
	void *m_base;
	
	unsigned int m_resultIdx;
	CkString *m_pResultString[10];

	unsigned int nextIdx(void);

	const wchar_t *rtnWideString(CkString *pStrObj);	


    public:
		
	CkWideCharBase();
	virtual ~CkWideCharBase();
	
	// BEGIN PUBLIC INTERFACE

	bool get_VerboseLogging(void);
	void put_VerboseLogging(bool b);

	bool SaveLastError(const wchar_t *path);

	void LastErrorXml(CkString &str);
	void LastErrorHtml(CkString &str);
	void LastErrorText(CkString &str);

	void get_LastErrorXml(CkString &str) { LastErrorXml(str); }
	void get_LastErrorHtml(CkString &str) { LastErrorHtml(str); }
	void get_LastErrorText(CkString &str) { LastErrorText(str); }

	const wchar_t *lastErrorText(void);
	const wchar_t *lastErrorXml(void);
	const wchar_t *lastErrorHtml(void);

	void get_DebugLogFilePath(CkString &str);
	void put_DebugLogFilePath(const wchar_t *newVal);

	const wchar_t *debugLogFilePath(void);

	void get_Version(CkString &str);
	const wchar_t *version(void);

	// END PUBLIC INTERFACE

	void *getImpl(void) const;

	// The following method(s) should not be called by an application.
	// They for internal use only.
	void setLastErrorProgrammingLanguage(int v);

    };

#ifndef __sun__
#pragma pack (pop)
#endif

#endif
	
