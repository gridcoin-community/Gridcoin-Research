// CkFtpProgress.h: interface for the CkFtpProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CKFTPPROGRESS_H
#define _CKFTPPROGRESS_H


#include "CkFtp2Progress.h"

/*

  To receive progress events (callbacks), create a C++ class that 
  inherits this one and provides one or more overriding implementations 
  for the events you wish to receive.  Then pass your progress object
  to CkFtp2 methods such as PutFileWithProgress, GetFileWithProgress, etc.

  */
#ifndef __sun__
#pragma pack (push, 8)
#endif
 
// For consistency in naming, the CkFtpProgress is deprecated and CkFtp2Progress is used.
class CkFtpProgress : public CkFtp2Progress
{
    public:

	CkFtpProgress() { }
	virtual ~CkFtpProgress() { }


};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
