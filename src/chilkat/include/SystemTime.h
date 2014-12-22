// This header is NOT generated.

#ifndef _SYSTEMTIME_H_INCLUDED_
#define _SYSTEMTIME_H_INCLUDED_	

#if !defined(_WINBASE_) && !defined(_MINWINBASE_) && !defined(_WINBASE_H)
	
typedef struct _SYSTEMTIME
    {
    unsigned short wYear;
    unsigned short wMonth;
    unsigned short wDayOfWeek;
    unsigned short wDay;
    unsigned short wHour;
    unsigned short wMinute;
    unsigned short wSecond;
    unsigned short wMilliseconds;
    
    // A flag that indicates whether daylight saving time is in effect at the time described. The value is positive if daylight saving time is in effect, zero if it is not, and negative if the information is not available.
    int m_isdst;
    } 	SYSTEMTIME;

#endif
	
#endif
