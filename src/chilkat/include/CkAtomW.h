// CkAtomW.h: interface for the CkAtomW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkAtomW_H
#define _CkAtomW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkDateTimeW;
class CkBaseProgressW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkAtomW
class CK_VISIBLE_PUBLIC CkAtomW  : public CkWideCharBase
{
    private:
	bool m_cbOwned;
	CkBaseProgressW *m_callback;

	// Don't allow assignment or copying these objects.
	CkAtomW(const CkAtomW &);
	CkAtomW &operator=(const CkAtomW &);

    public:
	CkAtomW(void);
	virtual ~CkAtomW(void);

	static CkAtomW *createNew(void);
	

	CkAtomW(bool bCallbackOwned);
	static CkAtomW *createNew(bool bCallbackOwned);

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	CkBaseProgressW *get_EventCallbackObject(void) const;
	void put_EventCallbackObject(CkBaseProgressW *progress);


	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// Number of entries in the Atom document.
	int get_NumEntries(void);



	// ----------------------
	// Methods
	// ----------------------
	// Adds a new element to the Atom document. The tag is a string such as "title",
	// "subtitle", "summary", etc. Returns the index of the element added, or -1 for
	// failure.
	int AddElement(const wchar_t *tag, const wchar_t *value);

	// Adds a new date-formatted element to the Atom document. The tag is a string
	// such as "created", "modified", "issued", etc. Returns the index of the element
	// added, or -1 for failure.
	int AddElementDate(const wchar_t *tag, SYSTEMTIME &dateTime);

	// Adds a new date-formatted element to the Atom document. The tag is a string
	// such as "created", "modified", "issued", etc. The  dateTimeStr should be an RFC822
	// formatted date/time string such as "Tue, 25 Sep 2012 12:25:32 -0500". Returns
	// the index of the element added, or -1 for failure.
	int AddElementDateStr(const wchar_t *tag, const wchar_t *dateTimeStr);

	// Adds a new date-formatted element to the Atom document. The tag is a string such
	// as "created", "modified", "issued", etc. Returns the index of the element added,
	// or -1 for failure.
	int AddElementDt(const wchar_t *tag, CkDateTimeW &dateTime);

	// Adds a new HTML formatted element to the Atom document. Returns the index of the
	// element added, or -1 for failure.
	int AddElementHtml(const wchar_t *tag, const wchar_t *htmlStr);

	// Adds a new XHTML formatted element to the Atom document. Returns the index of
	// the element added, or -1 for failure.
	int AddElementXHtml(const wchar_t *tag, const wchar_t *xmlStr);

	// Adds a new XML formatted element to the Atom document. Returns the index of the
	// element added, or -1 for failure.
	int AddElementXml(const wchar_t *tag, const wchar_t *xmlStr);

	// Adds an "entry" Atom XML document to the caller's Atom document.
	void AddEntry(const wchar_t *xmlStr);

	// Adds a link to the Atom document.
	void AddLink(const wchar_t *rel, const wchar_t *href, const wchar_t *title, const wchar_t *typ);

	// Adds a person to the Atom document. The tag should be a string such as "author",
	// "contributor", etc. If a piece of information is not known, an empty string or
	// NULL value may be passed.
	void AddPerson(const wchar_t *tag, const wchar_t *name, const wchar_t *uri, const wchar_t *email);

	// Removes the Nth occurance of a given element from the Atom document. Indexing
	// begins at 0. For example, to remove the 2nd category, set tag = "category" and
	// index = 1.
	void DeleteElement(const wchar_t *tag, int index);

	// Remove an attribute from an element.The index should be 0 unless there are
	// multiple elements having the same tag, in which case it selects the Nth
	// occurrence based on the index ( 0 = first occurrence ).
	void DeleteElementAttr(const wchar_t *tag, int index, const wchar_t *attrName);

	// Deletes a person from the Atom document. The tag is a string such as "author".
	// The index should be 0 unless there are multiple elements having the same tag, in
	// which case it selects the Nth occurrence based on the index. For example,
	// DeletePerson("author",2) deletes the 3rd author.
	void DeletePerson(const wchar_t *tag, int index);

	// Download an Atom feed from the Internet and load it into the Atom object.
	bool DownloadAtom(const wchar_t *url);

	// Returns the content of the Nth element having a specified tag.
	bool GetElement(const wchar_t *tag, int index, CkString &outStr);
	// Returns the content of the Nth element having a specified tag.
	const wchar_t *getElement(const wchar_t *tag, int index);
	// Returns the content of the Nth element having a specified tag.
	const wchar_t *element(const wchar_t *tag, int index);

	// Returns the value of an element's attribute. The element is selected by the tag
	// name and the index (the Nth element having a specific tag) and the attribute is
	// selected by name.
	bool GetElementAttr(const wchar_t *tag, int index, const wchar_t *attrName, CkString &outStr);
	// Returns the value of an element's attribute. The element is selected by the tag
	// name and the index (the Nth element having a specific tag) and the attribute is
	// selected by name.
	const wchar_t *getElementAttr(const wchar_t *tag, int index, const wchar_t *attrName);
	// Returns the value of an element's attribute. The element is selected by the tag
	// name and the index (the Nth element having a specific tag) and the attribute is
	// selected by name.
	const wchar_t *elementAttr(const wchar_t *tag, int index, const wchar_t *attrName);

	// The number of elements having a specific tag.
	int GetElementCount(const wchar_t *tag);

	// Returns an element's value as a date/time.
	bool GetElementDate(const wchar_t *tag, int index, SYSTEMTIME &outSysTime);

	// Returns an element's value as a date/time in an RFC822 formatted string, such as
	// such as "Tue, 25 Sep 2012 12:25:32 -0500".
	bool GetElementDateStr(const wchar_t *tag, int index, CkString &outStr);
	// Returns an element's value as a date/time in an RFC822 formatted string, such as
	// such as "Tue, 25 Sep 2012 12:25:32 -0500".
	const wchar_t *getElementDateStr(const wchar_t *tag, int index);
	// Returns an element's value as a date/time in an RFC822 formatted string, such as
	// such as "Tue, 25 Sep 2012 12:25:32 -0500".
	const wchar_t *elementDateStr(const wchar_t *tag, int index);

	// Returns an element's value as a date/time object.
	// The caller is responsible for deleting the object returned by this method.
	CkDateTimeW *GetElementDt(const wchar_t *tag, int index);

	// Returns the Nth entry as an Atom object. (Indexing begins at 0)
	// The caller is responsible for deleting the object returned by this method.
	CkAtomW *GetEntry(int index);

	// Returns the href attribute of the link having a specified "rel" attribute (such
	// as "service.feed", "alternate", etc.).
	bool GetLinkHref(const wchar_t *relName, CkString &outStr);
	// Returns the href attribute of the link having a specified "rel" attribute (such
	// as "service.feed", "alternate", etc.).
	const wchar_t *getLinkHref(const wchar_t *relName);
	// Returns the href attribute of the link having a specified "rel" attribute (such
	// as "service.feed", "alternate", etc.).
	const wchar_t *linkHref(const wchar_t *relName);

	// Returns a piece of information about a person. To get the 2nd author's name,
	// call GetPersonInfo("author",1,"name").
	bool GetPersonInfo(const wchar_t *tag, int index, const wchar_t *tag2, CkString &outStr);
	// Returns a piece of information about a person. To get the 2nd author's name,
	// call GetPersonInfo("author",1,"name").
	const wchar_t *getPersonInfo(const wchar_t *tag, int index, const wchar_t *tag2);
	// Returns a piece of information about a person. To get the 2nd author's name,
	// call GetPersonInfo("author",1,"name").
	const wchar_t *personInfo(const wchar_t *tag, int index, const wchar_t *tag2);

	// Returns the value of an attribute on the top-level XML node. The tag of a
	// top-level Atom XML node is typically "feed" or "entry", and it might have
	// attributes such as "xmlns" and "xml:lang".
	bool GetTopAttr(const wchar_t *attrName, CkString &outStr);
	// Returns the value of an attribute on the top-level XML node. The tag of a
	// top-level Atom XML node is typically "feed" or "entry", and it might have
	// attributes such as "xmlns" and "xml:lang".
	const wchar_t *getTopAttr(const wchar_t *attrName);
	// Returns the value of an attribute on the top-level XML node. The tag of a
	// top-level Atom XML node is typically "feed" or "entry", and it might have
	// attributes such as "xmlns" and "xml:lang".
	const wchar_t *topAttr(const wchar_t *attrName);

	// True (1) if the element exists in the Atom document. Otherwise 0.
	bool HasElement(const wchar_t *tag);

	// Loads the Atom document from an XML string.
	bool LoadXml(const wchar_t *xmlStr);

	// Initializes the Atom document to be a new "entry".
	void NewEntry(void);

	// Initializes the Atom document to be a new "feed".
	void NewFeed(void);

	// Adds or replaces an attribute on an element.
	void SetElementAttr(const wchar_t *tag, int index, const wchar_t *attrName, const wchar_t *attrValue);

	// Adds or replaces an attribute on the top-level XML node of the Atom document.
	void SetTopAttr(const wchar_t *attrName, const wchar_t *value);

	// Serializes the Atom document to an XML string.
	bool ToXmlString(CkString &outStr);
	// Serializes the Atom document to an XML string.
	const wchar_t *toXmlString(void);

	// Replaces the content of an element.
	void UpdateElement(const wchar_t *tag, int index, const wchar_t *value);

	// Replaces the content of a date-formatted element.
	void UpdateElementDate(const wchar_t *tag, int index, SYSTEMTIME &dateTime);

	// Replaces the content of a date-formatted element. The  index should be an RFC822
	// formatted date/time string.
	void UpdateElementDateStr(const wchar_t *tag, int index, const wchar_t *dateTimeStr);

	// Replaces the content of a date-formatted element.
	void UpdateElementDt(const wchar_t *tag, int index, CkDateTimeW &dateTime);

	// Replaces the content of an HTML element.
	void UpdateElementHtml(const wchar_t *tag, int index, const wchar_t *htmlStr);

	// Replaces the content of an XHTML element.
	void UpdateElementXHtml(const wchar_t *tag, int index, const wchar_t *xmlStr);

	// Replaces the content of an XML element.
	void UpdateElementXml(const wchar_t *tag, int index, const wchar_t *xmlStr);

	// Replaces the content of a person. To update the 3rd author, call
	// UpdatePerson("author",2,"new name","new URL","new email"). If a piece of
	// information is not known, pass an empty string or a NULL.
	void UpdatePerson(const wchar_t *tag, int index, const wchar_t *name, const wchar_t *uri, const wchar_t *email);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
