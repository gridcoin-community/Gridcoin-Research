// CkAtom.h: interface for the CkAtom class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkAtom_H
#define _CkAtom_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkDateTime;
class CkBaseProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkAtom
class CK_VISIBLE_PUBLIC CkAtom  : public CkMultiByteBase
{
    private:
	CkBaseProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkAtom(const CkAtom &);
	CkAtom &operator=(const CkAtom &);

    public:
	CkAtom(void);
	virtual ~CkAtom(void);

	static CkAtom *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	CkBaseProgress *get_EventCallbackObject(void) const;
	void put_EventCallbackObject(CkBaseProgress *progress);


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
	int AddElement(const char *tag, const char *value);

	// Adds a new date-formatted element to the Atom document. The tag is a string
	// such as "created", "modified", "issued", etc. Returns the index of the element
	// added, or -1 for failure.
	int AddElementDate(const char *tag, SYSTEMTIME &dateTime);

	// Adds a new date-formatted element to the Atom document. The tag is a string
	// such as "created", "modified", "issued", etc. The  dateTimeStr should be an RFC822
	// formatted date/time string such as "Tue, 25 Sep 2012 12:25:32 -0500". Returns
	// the index of the element added, or -1 for failure.
	int AddElementDateStr(const char *tag, const char *dateTimeStr);

	// Adds a new date-formatted element to the Atom document. The tag is a string such
	// as "created", "modified", "issued", etc. Returns the index of the element added,
	// or -1 for failure.
	int AddElementDt(const char *tag, CkDateTime &dateTime);

	// Adds a new HTML formatted element to the Atom document. Returns the index of the
	// element added, or -1 for failure.
	int AddElementHtml(const char *tag, const char *htmlStr);

	// Adds a new XHTML formatted element to the Atom document. Returns the index of
	// the element added, or -1 for failure.
	int AddElementXHtml(const char *tag, const char *xmlStr);

	// Adds a new XML formatted element to the Atom document. Returns the index of the
	// element added, or -1 for failure.
	int AddElementXml(const char *tag, const char *xmlStr);

	// Adds an "entry" Atom XML document to the caller's Atom document.
	void AddEntry(const char *xmlStr);

	// Adds a link to the Atom document.
	void AddLink(const char *rel, const char *href, const char *title, const char *typ);

	// Adds a person to the Atom document. The tag should be a string such as "author",
	// "contributor", etc. If a piece of information is not known, an empty string or
	// NULL value may be passed.
	void AddPerson(const char *tag, const char *name, const char *uri, const char *email);

	// Removes the Nth occurance of a given element from the Atom document. Indexing
	// begins at 0. For example, to remove the 2nd category, set tag = "category" and
	// index = 1.
	void DeleteElement(const char *tag, int index);

	// Remove an attribute from an element.The index should be 0 unless there are
	// multiple elements having the same tag, in which case it selects the Nth
	// occurrence based on the index ( 0 = first occurrence ).
	void DeleteElementAttr(const char *tag, int index, const char *attrName);

	// Deletes a person from the Atom document. The tag is a string such as "author".
	// The index should be 0 unless there are multiple elements having the same tag, in
	// which case it selects the Nth occurrence based on the index. For example,
	// DeletePerson("author",2) deletes the 3rd author.
	void DeletePerson(const char *tag, int index);

	// Download an Atom feed from the Internet and load it into the Atom object.
	bool DownloadAtom(const char *url);

	// Returns the content of the Nth element having a specified tag.
	bool GetElement(const char *tag, int index, CkString &outStr);
	// Returns the content of the Nth element having a specified tag.
	const char *getElement(const char *tag, int index);
	// Returns the content of the Nth element having a specified tag.
	const char *element(const char *tag, int index);

	// Returns the value of an element's attribute. The element is selected by the tag
	// name and the index (the Nth element having a specific tag) and the attribute is
	// selected by name.
	bool GetElementAttr(const char *tag, int index, const char *attrName, CkString &outStr);
	// Returns the value of an element's attribute. The element is selected by the tag
	// name and the index (the Nth element having a specific tag) and the attribute is
	// selected by name.
	const char *getElementAttr(const char *tag, int index, const char *attrName);
	// Returns the value of an element's attribute. The element is selected by the tag
	// name and the index (the Nth element having a specific tag) and the attribute is
	// selected by name.
	const char *elementAttr(const char *tag, int index, const char *attrName);

	// The number of elements having a specific tag.
	int GetElementCount(const char *tag);

	// Returns an element's value as a date/time.
	bool GetElementDate(const char *tag, int index, SYSTEMTIME &outSysTime);

	// Returns an element's value as a date/time in an RFC822 formatted string, such as
	// such as "Tue, 25 Sep 2012 12:25:32 -0500".
	bool GetElementDateStr(const char *tag, int index, CkString &outStr);
	// Returns an element's value as a date/time in an RFC822 formatted string, such as
	// such as "Tue, 25 Sep 2012 12:25:32 -0500".
	const char *getElementDateStr(const char *tag, int index);
	// Returns an element's value as a date/time in an RFC822 formatted string, such as
	// such as "Tue, 25 Sep 2012 12:25:32 -0500".
	const char *elementDateStr(const char *tag, int index);

	// Returns an element's value as a date/time object.
	// The caller is responsible for deleting the object returned by this method.
	CkDateTime *GetElementDt(const char *tag, int index);

	// Returns the Nth entry as an Atom object. (Indexing begins at 0)
	// The caller is responsible for deleting the object returned by this method.
	CkAtom *GetEntry(int index);

	// Returns the href attribute of the link having a specified "rel" attribute (such
	// as "service.feed", "alternate", etc.).
	bool GetLinkHref(const char *relName, CkString &outStr);
	// Returns the href attribute of the link having a specified "rel" attribute (such
	// as "service.feed", "alternate", etc.).
	const char *getLinkHref(const char *relName);
	// Returns the href attribute of the link having a specified "rel" attribute (such
	// as "service.feed", "alternate", etc.).
	const char *linkHref(const char *relName);

	// Returns a piece of information about a person. To get the 2nd author's name,
	// call GetPersonInfo("author",1,"name").
	bool GetPersonInfo(const char *tag, int index, const char *tag2, CkString &outStr);
	// Returns a piece of information about a person. To get the 2nd author's name,
	// call GetPersonInfo("author",1,"name").
	const char *getPersonInfo(const char *tag, int index, const char *tag2);
	// Returns a piece of information about a person. To get the 2nd author's name,
	// call GetPersonInfo("author",1,"name").
	const char *personInfo(const char *tag, int index, const char *tag2);

	// Returns the value of an attribute on the top-level XML node. The tag of a
	// top-level Atom XML node is typically "feed" or "entry", and it might have
	// attributes such as "xmlns" and "xml:lang".
	bool GetTopAttr(const char *attrName, CkString &outStr);
	// Returns the value of an attribute on the top-level XML node. The tag of a
	// top-level Atom XML node is typically "feed" or "entry", and it might have
	// attributes such as "xmlns" and "xml:lang".
	const char *getTopAttr(const char *attrName);
	// Returns the value of an attribute on the top-level XML node. The tag of a
	// top-level Atom XML node is typically "feed" or "entry", and it might have
	// attributes such as "xmlns" and "xml:lang".
	const char *topAttr(const char *attrName);

	// True (1) if the element exists in the Atom document. Otherwise 0.
	bool HasElement(const char *tag);

	// Loads the Atom document from an XML string.
	bool LoadXml(const char *xmlStr);

	// Initializes the Atom document to be a new "entry".
	void NewEntry(void);

	// Initializes the Atom document to be a new "feed".
	void NewFeed(void);

	// Adds or replaces an attribute on an element.
	void SetElementAttr(const char *tag, int index, const char *attrName, const char *attrValue);

	// Adds or replaces an attribute on the top-level XML node of the Atom document.
	void SetTopAttr(const char *attrName, const char *value);

	// Serializes the Atom document to an XML string.
	bool ToXmlString(CkString &outStr);
	// Serializes the Atom document to an XML string.
	const char *toXmlString(void);

	// Replaces the content of an element.
	void UpdateElement(const char *tag, int index, const char *value);

	// Replaces the content of a date-formatted element.
	void UpdateElementDate(const char *tag, int index, SYSTEMTIME &dateTime);

	// Replaces the content of a date-formatted element. The  index should be an RFC822
	// formatted date/time string.
	void UpdateElementDateStr(const char *tag, int index, const char *dateTimeStr);

	// Replaces the content of a date-formatted element.
	void UpdateElementDt(const char *tag, int index, CkDateTime &dateTime);

	// Replaces the content of an HTML element.
	void UpdateElementHtml(const char *tag, int index, const char *htmlStr);

	// Replaces the content of an XHTML element.
	void UpdateElementXHtml(const char *tag, int index, const char *xmlStr);

	// Replaces the content of an XML element.
	void UpdateElementXml(const char *tag, int index, const char *xmlStr);

	// Replaces the content of a person. To update the 3rd author, call
	// UpdatePerson("author",2,"new name","new URL","new email"). If a piece of
	// information is not known, pass an empty string or a NULL.
	void UpdatePerson(const char *tag, int index, const char *name, const char *uri, const char *email);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
