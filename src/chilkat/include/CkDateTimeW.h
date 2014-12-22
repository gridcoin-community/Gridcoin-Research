// CkDateTimeW.h: interface for the CkDateTimeW class.
//
//////////////////////////////////////////////////////////////////////



#ifndef _CkDateTimeW_H
#define _CkDateTimeW_H
	
#include "chilkatDefs.h"

#include <time.h>

#include "CkString.h"
#include "CkWideCharBase.h"




#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkDateTimeW
class CkDateTimeW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkDateTimeW(const CkDateTimeW &);
	CkDateTimeW &operator=(const CkDateTimeW &);

    public:
	CkDateTimeW(void);
	CkDateTimeW(bool bForDso);
	virtual ~CkDateTimeW(void);

	static CkDateTimeW *createNew(void);
	//static CkDateTimeW *createNew(bool bForDso);
	void inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// For the current system's timezone, returns the number of seconds offset from UTC
	// for this date/time. The offset includes daylight savings adjustment. Local
	// timezones west of UTC return a negative offset.
	int get_UtcOffset(void);

	// This is the Daylight Saving Time flag. It can have one of three possible values:
	// 1, 0, or -1. It has the value 1 if Daylight Saving Time is in effect, 0 if
	// Daylight Saving Time is not in effect, and -1 if the information is not
	// available.
	// 
	// Note: This is NOT the DST for the current system time. It is the DST that was in
	// effect at the date value contained in this object.
	// 
	int get_IsDst(void);



	// ----------------------
	// Methods
	// ----------------------
	// Adds an integer number of days to the date/time. To subtract days, pass a
	// negative integer.
	bool AddDays(int numDays);

	// Loads the date/time with a string having the format as produced by the Serialize
	// method, which is a string of SPACE separated integers containing (in this order)
	// year, month, day, hour, minutes, seconds, and a UTC flag having the value of
	// 1/0.
	void DeSerialize(const wchar_t *serializedDateTime);

	// Returns the date/time as a 64-bit integer .NET DateTime value.
	// 
	// bLocal indicates whether a local or UTC time is returned.
	// 
	// This is a date and time expressed in the number of 100-nanosecond intervals that
	// have elapsed since January 1, 0001 at 00:00:00.000 in the Gregorian calendar.
	// 
	// The DateTime value type represents dates and times with values ranging from
	// 12:00:00 midnight, January 1, 0001 Anno Domini (Common Era) through 11:59:59
	// P.M., December 31, 9999 A.D. (C.E.).
	// 
	// Time values are measured in 100-nanosecond units called ticks, and a particular
	// date is the number of ticks since 12:00 midnight, January 1, 0001 A.D. (C.E.) in
	// the GregorianCalendar calendar (excluding ticks that would be added by leap
	// seconds). For example, a ticks value of 31241376000000000L represents the date,
	// Friday, January 01, 0100 12:00:00 midnight. A DateTime value is always expressed
	// in the context of an explicit or default calendar.
	// 
	__int64 GetAsDateTimeTicks(bool bLocal);

#if !defined(CK_USE_UINT_T)
	// Returns the date/time as a 32-bit DOS date/time bitmask.
	// 
	// bLocal indicates whether a local or UTC time is returned.
	// 
	// The DOS date/time format is a bitmask:
	// 
	// 			   24                16                 8                 0
	// 	    +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
	// 	    |Y|Y|Y|Y|Y|Y|Y|M| |M|M|M|D|D|D|D|D| |h|h|h|h|h|m|m|m| |m|m|m|s|s|s|s|s|
	// 	    +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
	// 	     \___________/\________/\_________/ \________/\____________/\_________/
	// 		 year        month       day      hour       minute        second
	// 
	// The year is stored as an offset from 1980. Seconds are stored in two-second
	// increments. (So if the "second" value is 15, it actually represents 30 seconds.)
	// 
	unsigned long GetAsDosDate(bool bLocal);
#endif

	// Returns the date/time in a Windows FILETIME structure.
	// 
	// bLocal indicates whether a local or UTC time is returned.
	// 
	// For non-Windows systems, the FILETIME structure is defined in the FileTime.h
	// header provided in the Chilkat C/C++ libs distribution. The structure is defined
	// as follows:
	// typedef struct _FILETIME
	//     {
	//     unsigned long dwLowDateTime;
	//     unsigned long dwHighDateTime;
	//     } 	FILETIME;
	// 
	void GetAsFileTime(bool bLocal, FILETIME &fTime);

	// Returns the date/time in a Windows OLE "DATE" format.
	// 
	// bLocal indicates whether a local or UTC time is returned.
	// 
	// The OLE automation date format is a floating point value, counting days since
	// midnight 30 December 1899. Hours and minutes are represented as fractional days.
	// 
	double GetAsOleDate(bool bLocal);

	// Returns the date/time as an RFC822 formatted string. (An RFC822 format string is
	// what is found in the "Date" header field of an email.)
	// 
	// bLocal indicates whether a local or UTC time is returned.
	// 
	bool GetAsRfc822(bool bLocal, CkString &outStr);
	// Returns the date/time as an RFC822 formatted string. (An RFC822 format string is
	// what is found in the "Date" header field of an email.)
	// 
	// bLocal indicates whether a local or UTC time is returned.
	// 
	const wchar_t *getAsRfc822(bool bLocal);
	// Returns the date/time as an RFC822 formatted string. (An RFC822 format string is
	// what is found in the "Date" header field of an email.)
	// 
	// bLocal indicates whether a local or UTC time is returned.
	// 
	const wchar_t *asRfc822(bool bLocal);

	// Returns the date/time in a Windows SYSTEMTIME structure.
	// 
	// bLocal indicates whether the date/time returned is local or UTC.
	// 
	// For non-Windows systems, the SYSTEMTIME structure is defined in the SystemTime.h
	// header provided in the Chilkat C/C++ libs distribution. The structure is defined
	// as follows:
	// typedef struct _SYSTEMTIME
	//     {
	//     unsigned short wYear;
	//     unsigned short wMonth;
	//     unsigned short wDayOfWeek;
	//     unsigned short wDay;
	//     unsigned short wHour;
	//     unsigned short wMinute;
	//     unsigned short wSecond;
	//     unsigned short wMilliseconds;
	//     
	//     // A flag that indicates whether daylight saving time is in effect at the time described. 
	//     // The value is positive if daylight saving time is in effect, zero if it is not, 
	//     // and negative if the information is not available.
	//     int m_isdst;
	//     } 	SYSTEMTIME;
	// 
	void GetAsSystemTime(bool bLocal, SYSTEMTIME &outSysTime);

	// Returns the date/time in a Unix "struct tm" structure.
	void GetAsTmStruct(bool bLocal, struct tm &tmbuf);

	// Returns the date/time as a 32-bit Unix time.
	// 
	// bLocal indicates whether the date/time returned is local or UTC.
	// 
	// Note: With this format, there is a Y2038 problem that pertains to 32-bit signed
	// integers. There are approx 31.5 million seconds per year. The Unix time is
	// number of seconds since the Epoch, 1970-01-01 00:00:00 +0000 (UTC). In 2012,
	// it's 42 years since 1/1/1970, so the number of seconds is approx 1.3 billion. A
	// 32-bit signed integer ranges from -2,147,483,648 to 2,147,483,647 Therefore, if
	// a 32-bit signed integer is used, it turns negative in 2038.
	// 
	// The GetAsUnixTime64 and GetAsUnixTimeDbl methods are provided as solutions to
	// the Y2038 problem.
	// 
	time_t GetAsUnixTime(bool bLocal);

	// The same as GetUnixTime, except returns the date/time as a 64-bit integer.
	// 
	// bLocal indicates whether a local or UTC time is returned.
	// 
	__int64 GetAsUnixTime64(bool bLocal);

	// The same as GetUnixTime, except returns the date/time as a double.
	// 
	// bLocal indicates whether a local or UTC time is returned.
	// 
	double GetAsUnixTimeDbl(bool bLocal);

#if !defined(CK_USE_UINT_T)
	// Returns the high-order 16-bit integer of the date/time in DOS format. (See
	// GetAsDosDate for more information.)
	unsigned short GetDosDateHigh(bool bLocal);
#endif

#if !defined(CK_USE_UINT_T)
	// Returns the low-order 16-bit integer of the date/time in DOS format. (See
	// GetAsDosDate for more information.)
	unsigned short GetDosDateLow(bool bLocal);
#endif

	// Serializes the date/time to a us-ascii string that can be imported at a later
	// time via the DeSerialize method. The format of the string returned by this
	// method is not intended to match any published standard. It is formatted to a
	// string with SPACE separated integers containing (in this order) year, month,
	// day, hour, minutes, seconds, and a UTC flag having the value of 1/0.
	bool Serialize(CkString &outStr);
	// Serializes the date/time to a us-ascii string that can be imported at a later
	// time via the DeSerialize method. The format of the string returned by this
	// method is not intended to match any published standard. It is formatted to a
	// string with SPACE separated integers containing (in this order) year, month,
	// day, hour, minutes, seconds, and a UTC flag having the value of 1/0.
	const wchar_t *serialize(void);

	// Sets the date/time from the current system time.
	void SetFromCurrentSystemTime(void);

	// Sets the date/time from a .NET DateTime value represented in ticks. See
	// GetAsDateTimeTicks for more information.
	// 
	// bLocal indicates whether the passed in date/time is local or UTC.
	// 
	void SetFromDateTimeTicks(bool bLocal, __int64 ticks);

#if !defined(CK_USE_UINT_T)
	// Sets the date/time from a 32-bit DOS date/time bitmask. See GetAsDosDate for
	// more information.
	void SetFromDosDate(bool bLocal, unsigned long t);
#endif

#if !defined(CK_USE_UINT_T)
	// Sets the date/time from two 16-bit integers representing the high and low words
	// of a 32-bit DOS date/time bitmask. See GetAsDosDate for more information.
	// 
	// bLocal indicates whether the passed in date/time is local or UTC.
	// 
	void SetFromDosDate2(bool bLocal, unsigned short d, unsigned short t);
#endif

	// Sets the date/time from a Windows FILETIME structure.
	// 
	// bLocal indicates whether the passed in date/time is local or UTC.
	// 
	// For non-Windows systems, the FILETIME structure is defined in the FileTime.h
	// header provided in the Chilkat C/C++ libs distribution. The structure is defined
	// as follows:
	// typedef struct _FILETIME
	//     {
	//     unsigned long dwLowDateTime;
	//     unsigned long dwHighDateTime;
	//     } 	FILETIME;
	// 
	void SetFromFileTime(bool bLocal, FILETIME &fTime);

	// Sets the date/time from a Windows OLE "DATE" value.
	// 
	// bLocal indicates whether the passed in date/time is local or UTC.
	// 
	void SetFromOleDate(bool bLocal, double dt);

	// Sets the date/time from an RFC822 date/time formatted string.
	bool SetFromRfc822(const wchar_t *rfc822Str);

	// Sets the date/time from a Windows SYSTEMTIME structure.
	// 
	// bLocal indicates whether the passed in date/time is local or UTC.
	// 
	// For non-Windows systems, the SYSTEMTIME structure is defined in the SystemTime.h
	// header provided in the Chilkat C/C++ libs distribution. The structure is defined
	// as follows:
	// typedef struct _SYSTEMTIME
	//     {
	//     unsigned short wYear;
	//     unsigned short wMonth;
	//     unsigned short wDayOfWeek;
	//     unsigned short wDay;
	//     unsigned short wHour;
	//     unsigned short wMinute;
	//     unsigned short wSecond;
	//     unsigned short wMilliseconds;
	//     
	//     // A flag that indicates whether daylight saving time is in effect at the time described. 
	//     // The value is positive if daylight saving time is in effect, zero if it is not, 
	//     // and negative if the information is not available.
	//     int m_isdst;
	//     } 	SYSTEMTIME;
	// 
	void SetFromSystemTime(bool bLocal, SYSTEMTIME &sysTime);

	// Sets the date/time from a Unix "struct tm" structure.
	void SetFromTmStruct(bool bLocal, struct tm &tmbuf);

	// Sets the date/time from a 32-bit UNIX time value. (See GetAsUnixTime for
	// information about the Y2038 problem.)
	// 
	// bLocal indicates whether the passed in date/time is local or UTC.
	// 
	void SetFromUnixTime(bool bLocal, time_t t);

	// The same as SetFromUnixTime, except that it uses a 64-bit integer to solve the
	// Y2038 problem. (See GetAsUnixTime for more information about Y2038).
	// 
	// bLocal indicates whether the passed in date/time is local or UTC.
	// 
	void SetFromUnixTime64(bool bLocal, __int64 t);

	// The same as SetFromUnixTime, except that it uses a double to solve the Y2038
	// problem. (See GetAsUnixTime for more information about Y2038).
	// 
	// bLocal indicates whether the passed in date/time is local or UTC.
	// 
	void SetFromUnixTimeDbl(bool bLocal, double t);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
