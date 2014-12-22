// CkMailProgress.h: interface for the CkMailProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CkMailProgress_H
#define _CkMailProgress_H


#include "CkMailManProgress.h"

/*

  To receive progress events (callbacks), create a C++ class that 
  inherits this one and provides one or more overriding implementations 
  for the events you wish to receive.  

  */
#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// For naming consistency, CkMailManProgress is now used instead of CkMailProgress, which is deprecated.
class CkMailProgress  : public CkMailManProgress
{
    public:
	CkMailProgress() { }
	virtual ~CkMailProgress() { }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
