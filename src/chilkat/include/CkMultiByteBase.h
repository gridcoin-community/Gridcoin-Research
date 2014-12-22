#ifndef _CkMultiByteBase_H
#define _CkMultiByteBase_H
#pragma once

#ifndef __sun__
#pragma pack (push, 8)
#endif

#include "CkObject.h"

class CkString;

class CK_VISIBLE_PUBLIC CkMultiByteBase : public CkObject
{
    private:
	    
	// Disallow assignment or copying this object.
	CkMultiByteBase(const CkMultiByteBase &);
	CkMultiByteBase &operator=(const CkMultiByteBase &);

    protected:
	void *m_impl;	
	void *m_base;

	bool m_utf8;	// If true, all input "const char *" parameters are utf-8, otherwise they are ANSI strings.
	    
	unsigned int m_resultIdx;
	CkString *m_pResultString[10];

	unsigned int nextIdx(void);
    
	const char *rtnMbString(CkString *pStrObj);	

    public:
	    
	CkMultiByteBase(void);
	virtual ~CkMultiByteBase(void);
    
	// Applications should NOT call this method.  It is for internal use only.
	void clearResultStrings(void);

	// BEGIN PUBLIC INTERFACE

	bool get_Utf8(void) const;
	void put_Utf8(bool b);

	bool get_VerboseLogging(void);
	void put_VerboseLogging(bool b);

	bool SaveLastError(const char *path);

	void LastErrorXml(CkString &str);
	void LastErrorHtml(CkString &str);
	void LastErrorText(CkString &str);

	void get_LastErrorXml(CkString &str) { LastErrorXml(str); }
	void get_LastErrorHtml(CkString &str) { LastErrorHtml(str); }
	void get_LastErrorText(CkString &str) { LastErrorText(str); }

	const char *lastErrorText(void);
	const char *lastErrorXml(void);
	const char *lastErrorHtml(void);

	void get_DebugLogFilePath(CkString &str);
	void put_DebugLogFilePath(const char *newVal);

	const char *debugLogFilePath(void);

	void get_Version(CkString &str);
	const char *version(void);

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
	
