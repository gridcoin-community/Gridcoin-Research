// CkMailboxes.h: interface for the CkMailboxes class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkMailboxes_H
#define _CkMailboxes_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"




#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkMailboxes
class CK_VISIBLE_PUBLIC CkMailboxes  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkMailboxes(const CkMailboxes &);
	CkMailboxes &operator=(const CkMailboxes &);

    public:
	CkMailboxes(void);
	virtual ~CkMailboxes(void);

	static CkMailboxes *createNew(void);
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
	const char *getName(int index);
	// The name of the Nth mailbox in this collection. Indexing begins at 0.
	const char *name(int index);

	// Returns a comma-separated list of flags for the Nth mailbox. For example,
	// "\HasNoChildren,\Important".
	bool GetFlags(int index, CkString &outStr);
	// Returns a comma-separated list of flags for the Nth mailbox. For example,
	// "\HasNoChildren,\Important".
	const char *getFlags(int index);
	// Returns a comma-separated list of flags for the Nth mailbox. For example,
	// "\HasNoChildren,\Important".
	const char *flags(int index);

	// Returns true if the Nth mailbox has the specified flag set. The flag name is
	// case insensitive and should begin with a backslash character, such as
	// "\Flagged". The ARG1 is the index of the Nth mailbox.
	bool HasFlag(int index, const char *flagName);

	// Returns the number of flags for the Nth mailbox. Returns -1 if the ARG1 is out
	// of range.
	int GetNumFlags(int index);

	// Returns the name of the Nth flag for the Mth mailbox. The ARG1 is the index of
	// the mailbox. The ARG2 is the index of the flag.
	bool GetNthFlag(int index, int flagIndex, CkString &outStr);
	// Returns the name of the Nth flag for the Mth mailbox. The ARG1 is the index of
	// the mailbox. The ARG2 is the index of the flag.
	const char *getNthFlag(int index, int flagIndex);
	// Returns the name of the Nth flag for the Mth mailbox. The ARG1 is the index of
	// the mailbox. The ARG2 is the index of the flag.
	const char *nthFlag(int index, int flagIndex);

	// Returns the index of the mailbox having the specified name.
	int GetMailboxIndex(const char *mbxName);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
