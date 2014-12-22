// CkRss.h: interface for the CkRss class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkRss_H
#define _CkRss_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkBaseProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkRss
class CK_VISIBLE_PUBLIC CkRss  : public CkMultiByteBase
{
    private:
	CkBaseProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkRss(const CkRss &);
	CkRss &operator=(const CkRss &);

    public:
	CkRss(void);
	virtual ~CkRss(void);

	static CkRss *createNew(void);
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
	// The number of items in the channel.
	int get_NumItems(void);

	// The number of channels in the RSS document.
	int get_NumChannels(void);



	// ----------------------
	// Methods
	// ----------------------
	// Adds a new channel to the RSS document. Returns the Rss object representing the
	// Channel which can then be edited.
	// The caller is responsible for deleting the object returned by this method.
	CkRss *AddNewChannel(void);

	// Adds a new image to the RSS document. Returns the Rss object representing the
	// image, which can then be edited.
	// The caller is responsible for deleting the object returned by this method.
	CkRss *AddNewImage(void);

	// Adds a new Item to an Rss channel. Returns the Rss object representing the item
	// which can then be edited.
	// The caller is responsible for deleting the object returned by this method.
	CkRss *AddNewItem(void);

	// Downloads an RSS document from the Internet and populates the Rss object with
	// the contents.
	bool DownloadRss(const char *url);

	// Returns the value of a sub-element attribute. For example, to get the value of
	// the "isPermaLink" attribute of the "guid" sub-element, call
	// item.GetAttr("guid","isPermaLink").
	bool GetAttr(const char *tag, const char *attrName, CkString &outStr);
	// Returns the value of a sub-element attribute. For example, to get the value of
	// the "isPermaLink" attribute of the "guid" sub-element, call
	// item.GetAttr("guid","isPermaLink").
	const char *getAttr(const char *tag, const char *attrName);
	// Returns the value of a sub-element attribute. For example, to get the value of
	// the "isPermaLink" attribute of the "guid" sub-element, call
	// item.GetAttr("guid","isPermaLink").
	const char *attr(const char *tag, const char *attrName);

	// Returns the Nth channel of an RSS document. Usually there is only 1 channel per
	// document, so the index argument should be set to 0.
	// The caller is responsible for deleting the object returned by this method.
	CkRss *GetChannel(int index);

	// Return the number of sub-elements with a specific tag.
	int GetCount(const char *tag);

	// Return the value of a sub-element in date/time format.
	bool GetDate(const char *tag, SYSTEMTIME &outSysTime);

	// The same as GetDate, except the date/time is returned in RFC822 string format.
	bool GetDateStr(const char *tag, CkString &outStr);
	// The same as GetDate, except the date/time is returned in RFC822 string format.
	const char *getDateStr(const char *tag);
	// The same as GetDate, except the date/time is returned in RFC822 string format.
	const char *dateStr(const char *tag);

	// Return the image associated with the channel.
	// The caller is responsible for deleting the object returned by this method.
	CkRss *GetImage(void);

	// Return the value of a numeric sub-element as an integer.
	int GetInt(const char *tag);

	// Return the Nth item of a channel as an RSS object.
	// The caller is responsible for deleting the object returned by this method.
	CkRss *GetItem(int index);

	// Return the value of an sub-element as a string.
	bool GetString(const char *tag, CkString &outStr);
	// Return the value of an sub-element as a string.
	const char *getString(const char *tag);
	// Return the value of an sub-element as a string.
	const char *string(const char *tag);

	// Load an RSS document from a file.
	bool LoadRssFile(const char *filename);

	// Loads an RSS feed document from an in-memory string.
	bool LoadRssString(const char *rssString);

	// Get an attribute value for the Nth sub-element having a specific tag. As an
	// example, an RSS item may have several "category" sub-elements. To get the value
	// of the "domain" attribute for the 3rd category, call
	// MGetAttr("category",2,"domain").
	bool MGetAttr(const char *tag, int index, const char *attrName, CkString &outStr);
	// Get an attribute value for the Nth sub-element having a specific tag. As an
	// example, an RSS item may have several "category" sub-elements. To get the value
	// of the "domain" attribute for the 3rd category, call
	// MGetAttr("category",2,"domain").
	const char *mGetAttr(const char *tag, int index, const char *attrName);

	// Get the value of the Nth occurance of a sub-element. Indexing begins at 0.
	bool MGetString(const char *tag, int index, CkString &outStr);
	// Get the value of the Nth occurance of a sub-element. Indexing begins at 0.
	const char *mGetString(const char *tag, int index);

	// Set an attribute on the Nth occurance of a sub-element.
	bool MSetAttr(const char *tag, int idx, const char *attrName, const char *value);

	// Set the value of the Nth occurance of a sub-element. Indexing begins at 0.
	bool MSetString(const char *tag, int idx, const char *value);

	// Clears the RSS document.
	void NewRss(void);

	// Removes a sub-element from the RSS document.
	void Remove(const char *tag);

	// Sets the value of a sub-element attribute.
	void SetAttr(const char *tag, const char *attrName, const char *value);

	// Sets the value of a date/time sub-element.
	void SetDate(const char *tag, SYSTEMTIME &dateTime);

	// Sets the value of a date/time sub-element to the current system date/time.
	void SetDateNow(const char *tag);

	// The same as SetDate, except the date/time is passed as an RFC822 string.
	void SetDateStr(const char *tag, const char *dateTimeStr);

	// Sets the value of an integer sub-element.
	void SetInt(const char *tag, int value);

	// Sets the value of a sub-element.
	void SetString(const char *tag, const char *value);

	// Returns the RSS document as an XML string.
	bool ToXmlString(CkString &outStr);
	// Returns the RSS document as an XML string.
	const char *toXmlString(void);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
