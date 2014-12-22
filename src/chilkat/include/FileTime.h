// This header is NOT generated.

#ifndef _FILETIME_H_INCLUDED_
#define _FILETIME_H_INCLUDED_

// _WINBASE_H is defined by MinGW's winbase.h
#if !defined(_WINBASE_) && !defined(_MINWINBASE_) && !defined(_WINBASE_H)
	
typedef struct _FILETIME
    {
    unsigned long dwLowDateTime;
    unsigned long dwHighDateTime;
    } 	FILETIME;

#endif
	

#endif

