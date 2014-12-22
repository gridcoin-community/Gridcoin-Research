// CkMailboxesW.h: interface for the CkMailboxesW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkMailboxesW_H
#define _CkMailboxesW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"




#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkMailboxesW
class CK_VISIBLE_PUBLIC CkMailboxesW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkMailboxesW(const CkMailboxesW &);
	CkMailboxesW &operator=(const CkMailboxesW &);

    public:
	CkMailboxesW(void);
	virtual ~CkMailboxesW(void);

	static CkMailboxesW *createNew(void);
	

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The number of mailboxes in the collection.
	// 
	// Note: The Mailboxes class is for use with the Chilkat IMAP component.
	// 
	int get_Count(void);



	// ----------------------
	// Methods
	// ----------------------
	// Returns true if the Nth mailbox has inferiors (i.e. sub-mailboxes)
	bool HasInferiors(int index);

	// Returns true if the Nth mailbox is marked.
	bool IsMarked(int index);

	// Returns true if the Nth mailbox is selectable.
	bool IsSelectable(int index);

	// The name of the Nth mailbox in this collection. Indexing begins at 0.
	bool GetName(int index, CkString &outStrName);
	// The name of the Nth mailbox in this collection. Indexing begins at 0.
	const wchar_t *getName(int index);
	// The name of the Nth mailbox in this collection. Indexing begins at 0.
	const wchar_t *name(int index);

	// Returns a comma-separated list of flags for the Nth mailbox. For example,
	// "\HasNoChildren,\Important".
	bool GetFlags(int index, CkString &outStr);
	// Returns a comma-separated list of flags for the Nth mailbox. For example,
	// "\HasNoChildren,\Important".
	const wchar_t *getFlags(int index);
	// Returns a comma-separated list of flags for the Nth mailbox. For example,
	// "\HasNoChildren,\Important".
	const wchar_t *flags(int index);

	// Returns true if the Nth mailbox has the specified flag set. The flag name is
	// case insensitive and should begin with a backslash character, such as
	// "\Flagged". The ARG1 is the index of the Nth mailbox.
	bool HasFlag(int index, const wchar_t *flagName);

	// Returns the number of flags for the Nth mailbox. Returns -1 if the ARG1 is out
	// of range.
	int GetNumFlags(int index);

	// Returns the name of the Nth flag for the Mth mailbox. The ARG1 is the index of
	// the mailbox. The ARG2 is the index of the flag.
	bool GetNthFlag(int index, int flagIndex, CkString &outStr);
	// Returns the name of the Nth flag for the Mth mailbox. The ARG1 is the index of
	// the mailbox. The ARG2 is the index of the flag.
	const wchar_t *getNthFlag(int index, int flagIndex);
	// Returns the name of the Nth flag for the Mth mailbox. The ARG1 is the index of
	// the mailbox. The ARG2 is the index of the flag.
	const wchar_t *nthFlag(int index, int flagIndex);

	// Returns the index of the mailbox having the specified name.
	int GetMailboxIndex(const wchar_t *mbxName);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
