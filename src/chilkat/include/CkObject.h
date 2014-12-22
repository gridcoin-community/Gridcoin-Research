// This header is NOT generated.

#if !defined(_CKOBJECT_H_INCLUDED_)
#define _CKOBJECT_H_INCLUDED_

#include "chilkatDefs.h"

#ifndef __sun__
#pragma pack (push, 8)
#endif
 

class CK_VISIBLE_PUBLIC CkObject  
{
public:
	CkObject();
	virtual ~CkObject();

	// Scan sUtf8 and make sure it is a valid utf-8 string
	// that is assured of being handled correctly by 
	// NSString's stringWithUTF8String constructor.
	const char *objcUtf8(const char *sUtf8);
	void objcUtf8_free(void);

    private:
	char *m_utf8Str;

};

#ifndef __sun__
#pragma pack (pop)
#endif


#endif // !defined(_CKOBJECT_H_INCLUDED_)
