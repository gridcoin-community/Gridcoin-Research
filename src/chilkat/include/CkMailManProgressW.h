// CkMailManProgress.h: interface for the CkMailManProgress class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _CkMailManProgressW_H
#define _CkMailManProgressW_H


#include "CkBaseProgressW.h"

/*

  To receive progress events (callbacks), create a C++ class that 
  inherits this one and provides one or more overriding implementations 
  for the events you wish to receive.  

  */

// When creating an application class that inherits the CkMailManProgressW base class, use the CK_MAILPROGRESSW_API 
// definition to declare the overrides in the class header.  This has the effect that if for
// some unforeseen and unlikely reason the Chilkat event callback API changes, or if new
// callback methods are added in a future version, then you'll discover them at compile time
// after updating to the new Chilkat version.  
// For example:
/*
    class MyProgress : public CkMailManProgressW
    {
	public:
	    CK_MAILPROGRESSW_API

	...
    };
*/
#define CK_MAILPROGRESSW_API \
	void EmailReceived(const wchar_t *subject, \
			    const wchar_t *fromAddr, const wchar_t *fromName, \
			    const wchar_t *returnPath, \
			    const wchar_t *date, \
			    const wchar_t *uidl, \
			    int sizeInBytes);

#ifndef __sun__
#pragma pack (push, 8)
#endif
 
class CkMailManProgressW  : public CkBaseProgressW
{
    public:
	CkMailManProgressW() { }
	virtual ~CkMailManProgressW() { }

	// These event callbacks are now defined in CkBaseProgressW.
	//virtual void PercentDone(int pctDone, bool *abort) { }
	//virtual void AbortCheck(bool *abort) { }
	//virtual void ProgressInfo(const wchar_t *name, const wchar_t *value) { }

	virtual void EmailReceived(const wchar_t *subject, 
			    const wchar_t *fromAddr, const wchar_t *fromName, 
			    const wchar_t *returnPath, 
			    const wchar_t *date, 
			    const wchar_t *uidl, 
			    int sizeInBytes) { }


};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
