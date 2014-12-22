// CkDateTime.h: interface for the CkDateTime class.
//
//////////////////////////////////////////////////////////////////////

// This header is NOT generated.

#ifndef _CkDateTime_H
#define _CkDateTime_H

#include "chilkatDefs.h"

#include <time.h>

#include "SystemTime.h"
#include "FileTime.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

#ifndef __sun__
#pragma pack (push, 8)
#endif

 
// CLASS: CkDateTime
class CkDateTime  : public CkMultiByteBase
{
    private:
	// Don't allow assignment or copying these objects.
	CkDateTime(const CkDateTime &);
	CkDateTime &operator=(const CkDateTime &);


    public:
	CkDateTime(void *impl);

	CkDateTime();
	virtual ~CkDateTime();

	static CkDateTime *createNew(void);
	void inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	// BEGIN PUBLIC INTERFACE
	// ADDDAYS_BEGIN
	bool AddDays(int numDays);
	// ADDDAYS_END

	// DESERIALIZE_BEGIN
	void DeSerialize(const char *serializedDateTime);
	// DESERIALIZE_END

	// SERIALIZE_BEGIN
	bool Serialize(CkString &outStr);
	const char *serialize(void);
	// SERIALIZE_END


	void SetFromCurrentSystemTime(void);

	// bLocal indicates whether the date/time returned is local or UTC,
	// or if the date/time passed in is local or UTC.

	// Y2038 problem pertains to 32-bit signed integers.
	// There are approx 31.5 million seconds per year.
	// The Unix time is number of seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC). 
	// In 2012, it's 42 years since 1/1/1970, so the number of seconds
	// is approx 1.3 billion.  A 32-bit signed integer ranges from
	// -2,147,483,648 to 2,147,483,647
	// Therefore, if a 32-bit signed integer is used, it turns negative in 2038.

	// 32-bit Unix time.
	time_t GetAsUnixTime(bool bLocal);
	void SetFromUnixTime(bool bLocal, time_t t);

	// 64-bit Unix time.
	__int64 GetAsUnixTime64(bool bLocal);
	void SetFromUnixTime64(bool bLocal, __int64 t);

	// Unix time (double)
	double GetAsUnixTimeDbl(bool bLocal);
	void SetFromUnixTimeDbl(bool bLocal, double t);

	// struct tm
	void GetAsTmStruct(bool bLocal, struct tm &tmbuf);
	void SetFromTmStruct(bool bLocal, struct tm &tmbuf);

	// Windows SYSTEMTIME struct.
	void GetAsSystemTime(bool bLocal, SYSTEMTIME &outSysTime);
	void SetFromSystemTime(bool bLocal, SYSTEMTIME &sysTime);

	// Windows FILETIME struct.
	void GetAsFileTime(bool bLocal, FILETIME &fTime);
	void SetFromFileTime(bool bLocal, FILETIME &fTime);

	// The OLE automation date format is a floating point value, 
	// counting days since midnight 30 December 1899. Hours and minutes are 
	// represented as fractional days. 
	double GetAsOleDate(bool bLocal);
	void SetFromOleDate(bool bLocal, double dt);


//	The DOS date/time format is a bitmask:
//
//			   24                16                 8                 0
//	    +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
//	    |Y|Y|Y|Y|Y|Y|Y|M| |M|M|M|D|D|D|D|D| |h|h|h|h|h|m|m|m| |m|m|m|s|s|s|s|s|
//	    +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
//	     \___________/\________/\_________/ \________/\____________/\_________/
//		 year        month       day      hour       minute        second
//
//	The year is stored as an offset from 1980. Seconds are stored in two-second 
//	increments. (So if the "second" value is 15, it actually represents 30 seconds.)
//
//	These values are recorded in local time.
//
//	November 26, 2002 at 7:25p PST = 0x2D7A9B20.
//
//	To convert these values to something readable, convert it to a 
//	FILETIME via DosDateTimeToFileTime, then convert the FILETIME to 
//	something readable. 

#if !defined(CK_USE_UINT_T)
	unsigned long GetAsDosDate(bool bLocal);
	void SetFromDosDate(bool bLocal, unsigned long t);

	void SetFromDosDate2(bool bLocal, unsigned short d, unsigned short t);
	unsigned short GetDosDateHigh(bool bLocal);
	unsigned short GetDosDateLow(bool bLocal);
#endif

	// Do not use #else because of C++ --> C code generation...
#if defined(CK_USE_UINT_T)
	uint32_t GetAsDosDate(bool bLocal);
	void SetFromDosDate(bool bLocal, uint32_t t);

	void SetFromDosDate2(bool bLocal, uint16_t d, uint16_t t);
	uint16_t GetDosDateHigh(bool bLocal);
	uint16_t GetDosDateLow(bool bLocal);
#endif

	// .NET DateTime value.

	// A date and time expressed in the number of 100-nanosecond intervals that have 
	// elapsed since January 1, 0001 at 00:00:00.000 in the Gregorian calendar. 

	// The DateTime value type represents dates and times with values ranging from 
	// 12:00:00 midnight, January 1, 0001 Anno Domini (Common Era) through 11:59:59 P.M., 
	// December 31, 9999 A.D. (C.E.).

	// Time values are measured in 100-nanosecond units called ticks, and a particular 
	// date is the number of ticks since 12:00 midnight, January 1, 0001 A.D. (C.E.) 
	// in the GregorianCalendar calendar (excluding ticks that would be added by leap 
	// seconds). For example, a ticks value of 31241376000000000L represents the date, 
	// Friday, January 01, 0100 12:00:00 midnight. A DateTime value is always expressed 
	// in the context of an explicit or default calendar.

	__int64 GetAsDateTimeTicks(bool bLocal);
	void SetFromDateTimeTicks(bool bLocal, __int64 n);


	// ---------------------------------------------------------
	// Daylight savings and GMT/UTC offset from local timezone.
	// ---------------------------------------------------------

	// (for the current system's timezone)
	// Get the number of seconds offset from UTC for this date/time.
	// The offset includes daylight savings adjustment.
	// Local timezones west of UTC return a negative offset.
	int get_UtcOffset(void);

	// The Daylight Saving Time flag (tm_isdst) is greater than zero 
	// if Daylight Saving Time is in effect, zero if Daylight Saving Time 
	// is not in effect, and less than zero if the information is not available.
	// (This is NOT the DST for the current system time, but for the date value
	// contained in this object.)
	// Returns 1, 0, or -1
	int get_IsDst(void);




	// ----------------------
	// String formats.
	// ----------------------



	// GETASRFC822_BEGIN
	bool GetAsRfc822(bool bLocal, CkString &outStr);
	const char *getAsRfc822(bool bLocal);
	// GETASRFC822_END
	// SETFROMRFC822_BEGIN
	bool SetFromRfc822(const char *rfc822Str);
	// SETFROMRFC822_END

	// DATETIME_INSERT_POINT

	// END PUBLIC INTERFACE



};
#ifndef __sun__
#pragma pack (pop)
#endif



#endif


