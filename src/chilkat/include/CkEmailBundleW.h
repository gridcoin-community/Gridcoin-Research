// CkEmailBundleW.h: interface for the CkEmailBundleW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkEmailBundleW_H
#define _CkEmailBundleW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkEmailW;
class CkStringArrayW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkEmailBundleW
class CK_VISIBLE_PUBLIC CkEmailBundleW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkEmailBundleW(const CkEmailBundleW &);
	CkEmailBundleW &operator=(const CkEmailBundleW &);

    public:
	CkEmailBundleW(void);
	virtual ~CkEmailBundleW(void);

	static CkEmailBundleW *createNew(void);
	

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The number of emails in this bundle.
	int get_MessageCount(void);



	// ----------------------
	// Methods
	// ----------------------
	// Adds an email object to the bundle.
	bool AddEmail(const CkEmailW &email);

	// Returns the first email having a header field matching the headerFieldName and  headerFieldValue exactly
	// (case sensitive). If no matching email is found, returns _NULL_.
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *FindByHeader(const wchar_t *name, const wchar_t *value);

	// Returns the Nth Email in the bundle. The email returned is a copy of the email
	// in the bundle. Updating the email object returned by GetEmail has no effect on
	// the email within the bundle. To update/replace the email in the bundle, your
	// program should call GetEmail to get a copy, make modifications, call
	// RemoveEmailByIndex to remove the email (passing the same index used in the call
	// to GetEmail), and then call AddEmail to insert the new/modified email into the
	// bundle.
	// 
	// IMPORTANT: This method does NOT communicate with any mail server to download the
	// email. It simply returns the Nth email object that exists within it's in-memory
	// collection of email objects.
	// 
	// The caller is responsible for deleting the object returned by this method.
	CkEmailW *GetEmail(int index);

	// Returns a StringArray object containing UIDLs for all Email objects in the
	// bundle. UIDLs are only valid for emails retrieved from POP3 servers. An email on
	// a POP3 server has a "UIDL", an email on IMAP servers has a "UID". If the email
	// was retrieved from an IMAP server, the UID will be accessible via the
	// "ckx-imap-uid" header field.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *GetUidls(void);

	// Converts the email bundle to an XML document in memory. Returns the XML document
	// as a string.
	bool GetXml(CkString &outXml);
	// Converts the email bundle to an XML document in memory. Returns the XML document
	// as a string.
	const wchar_t *getXml(void);
	// Converts the email bundle to an XML document in memory. Returns the XML document
	// as a string.
	const wchar_t *xml(void);

	// Loads an email bundle from an XML file.
	bool LoadXml(const wchar_t *filename);

	// Loads an email bundle from an XML string.
	bool LoadXmlString(const wchar_t *xmlStr);

	// Removes an email from the bundle. This does not remove the email from the mail
	// server.
	bool RemoveEmail(const CkEmailW &email);

	// Removes the Nth email in a bundle. (Indexing begins at 0.)
	bool RemoveEmailByIndex(int index);

	// Converts each email to XML and persists the bundle to an XML file. The email
	// bundle can later be re-instantiated by calling MailMan.LoadXmlFile
	bool SaveXml(const wchar_t *filename);

	// Sorts emails in the bundle by date.
	void SortByDate(bool ascending);

	// Sorts emails in the bundle by recipient.
	void SortByRecipient(bool ascending);

	// Sorts emails in the bundle by sender.
	void SortBySender(bool ascending);

	// Sorts emails in the bundle by subject.
	void SortBySubject(bool ascending);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
