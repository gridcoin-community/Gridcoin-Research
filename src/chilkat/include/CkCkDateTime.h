// CkCkDateTime.h: interface for the CkCkDateTime class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated.

#ifndef _CkCkDateTime_H
#define _CkCkDateTime_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"




#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkCkDateTime
class CkCkDateTime  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkCkDateTime(const CkCkDateTime &);
	CkCkDateTime &operator=(const CkCkDateTime &);

    public:
	CkCkDateTime(void);
	virtual ~CkCkDateTime(void);

	static CkCkDateTime *createNew(void);
	void inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	int get_UtcOffset(void);

	int get_IsDst(void);



	// ----------------------
	// Methods
	// ----------------------
	bool AddDays(int numDays);

	void DeSerialize(const char *serializedDateTime);

	__int64 GetAsDateTimeTicks(bool bLocal);

#if !defined(CK_USE_UINT_T)
	unsigned long GetAsDosDate(bool bLocal);
#endif

	void GetAsFileTime(bool bLocal, FILETIME &fTime);

	double GetAsOleDate(bool bLocal);

	bool GetAsRfc822(bool bLocal, CkString &outStr);
	const char *getAsRfc822(bool bLocal);
	const char *asRfc822(bool bLocal);

	bool GetAsSystemTime(bool bLocal, SYSTEMTIME &outSysTime);

	void GetAsTmStruct(bool bLocal, struct tm &tmbuf);

	time_t GetAsUnixTime(bool bLocal);

	__int64 GetAsUnixTime64(bool bLocal);

	double GetAsUnixTimeDbl(bool bLocal);

#if !defined(CK_USE_UINT_T)
	unsigned short GetDosDateHigh(bool bLocal);
#endif

#if !defined(CK_USE_UINT_T)
	unsigned short GetDosDateLow(bool bLocal);
#endif

	bool Serialize(CkString &outStr);
	const char *serialize(void);

	void SetFromCurrentSystemTime(void);

	void SetFromDateTimeTicks(bool bLocal, __int64 ticks);

#if !defined(CK_USE_UINT_T)
	void SetFromDosDate(bool bLocal, unsigned long t);
#endif

#if !defined(CK_USE_UINT_T)
	void SetFromDosDate2(bool bLocal, unsigned short d, unsigned short t);
#endif

	void SetFromFileTime(bool bLocal, FILETIME &fTime);

	void SetFromOleDate(bool bLocal, double dt);

	bool SetFromRfc822(const char *rfc822Str);

	void SetFromSystemTime(bool bLocal, SYSTEMTIME &sysTime);

	void SetFromTmStruct(bool bLocal, struct tm &tmbuf);

	void SetFromUnixTime(bool bLocal, time_t t);

	void SetFromUnixTime64(bool bLocal, __int64 t);

	void SetFromUnixTimeDbl(bool bLocal, double t);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
