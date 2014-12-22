// CkRssW.h: interface for the CkRssW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkRssW_H
#define _CkRssW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkBaseProgressW;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkRssW
class CK_VISIBLE_PUBLIC CkRssW  : public CkWideCharBase
{
    private:
	bool m_cbOwned;
	CkBaseProgressW *m_callback;

	// Don't allow assignment or copying these objects.
	CkRssW(const CkRssW &);
	CkRssW &operator=(const CkRssW &);

    public:
	CkRssW(void);
	virtual ~CkRssW(void);

	static CkRssW *createNew(void);
	

	CkRssW(bool bCallbackOwned);
	static CkRssW *createNew(bool bCallbackOwned);

	
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
	CkRssW *AddNewChannel(void);

	// Adds a new image to the RSS document. Returns the Rss object representing the
	// image, which can then be edited.
	// The caller is responsible for deleting the object returned by this method.
	CkRssW *AddNewImage(void);

	// Adds a new Item to an Rss channel. Returns the Rss object representing the item
	// which can then be edited.
	// The caller is responsible for deleting the object returned by this method.
	CkRssW *AddNewItem(void);

	// Downloads an RSS document from the Internet and populates the Rss object with
	// the contents.
	bool DownloadRss(const wchar_t *url);

	// Returns the value of a sub-element attribute. For example, to get the value of
	// the "isPermaLink" attribute of the "guid" sub-element, call
	// item.GetAttr("guid","isPermaLink").
	bool GetAttr(const wchar_t *tag, const wchar_t *attrName, CkString &outStr);
	// Returns the value of a sub-element attribute. For example, to get the value of
	// the "isPermaLink" attribute of the "guid" sub-element, call
	// item.GetAttr("guid","isPermaLink").
	const wchar_t *getAttr(const wchar_t *tag, const wchar_t *attrName);
	// Returns the value of a sub-element attribute. For example, to get the value of
	// the "isPermaLink" attribute of the "guid" sub-element, call
	// item.GetAttr("guid","isPermaLink").
	const wchar_t *attr(const wchar_t *tag, const wchar_t *attrName);

	// Returns the Nth channel of an RSS document. Usually there is only 1 channel per
	// document, so the index argument should be set to 0.
	// The caller is responsible for deleting the object returned by this method.
	CkRssW *GetChannel(int index);

	// Return the number of sub-elements with a specific tag.
	int GetCount(const wchar_t *tag);

	// Return the value of a sub-element in date/time format.
	bool GetDate(const wchar_t *tag, SYSTEMTIME &outSysTime);

	// The same as GetDate, except the date/time is returned in RFC822 string format.
	bool GetDateStr(const wchar_t *tag, CkString &outStr);
	// The same as GetDate, except the date/time is returned in RFC822 string format.
	const wchar_t *getDateStr(const wchar_t *tag);
	// The same as GetDate, except the date/time is returned in RFC822 string format.
	const wchar_t *dateStr(const wchar_t *tag);

	// Return the image associated with the channel.
	// The caller is responsible for deleting the object returned by this method.
	CkRssW *GetImage(void);

	// Return the value of a numeric sub-element as an integer.
	int GetInt(const wchar_t *tag);

	// Return the Nth item of a channel as an RSS object.
	// The caller is responsible for deleting the object returned by this method.
	CkRssW *GetItem(int index);

	// Return the value of an sub-element as a string.
	bool GetString(const wchar_t *tag, CkString &outStr);
	// Return the value of an sub-element as a string.
	const wchar_t *getString(const wchar_t *tag);
	// Return the value of an sub-element as a string.
	const wchar_t *string(const wchar_t *tag);

	// Load an RSS document from a file.
	bool LoadRssFile(const wchar_t *filename);

	// Loads an RSS feed document from an in-memory string.
	bool LoadRssString(const wchar_t *rssString);

	// Get an attribute value for the Nth sub-element having a specific tag. As an
	// example, an RSS item may have several "category" sub-elements. To get the value
	// of the "domain" attribute for the 3rd category, call
	// MGetAttr("category",2,"domain").
	bool MGetAttr(const wchar_t *tag, int index, const wchar_t *attrName, CkString &outStr);
	// Get an attribute value for the Nth sub-element having a specific tag. As an
	// example, an RSS item may have several "category" sub-elements. To get the value
	// of the "domain" attribute for the 3rd category, call
	// MGetAttr("category",2,"domain").
	const wchar_t *mGetAttr(const wchar_t *tag, int index, const wchar_t *attrName);

	// Get the value of the Nth occurance of a sub-element. Indexing begins at 0.
	bool MGetString(const wchar_t *tag, int index, CkString &outStr);
	// Get the value of the Nth occurance of a sub-element. Indexing begins at 0.
	const wchar_t *mGetString(const wchar_t *tag, int index);

	// Set an attribute on the Nth occurance of a sub-element.
	bool MSetAttr(const wchar_t *tag, int idx, const wchar_t *attrName, const wchar_t *value);

	// Set the value of the Nth occurance of a sub-element. Indexing begins at 0.
	bool MSetString(const wchar_t *tag, int idx, const wchar_t *value);

	// Clears the RSS document.
	void NewRss(void);

	// Removes a sub-element from the RSS document.
	void Remove(const wchar_t *tag);

	// Sets the value of a sub-element attribute.
	void SetAttr(const wchar_t *tag, const wchar_t *attrName, const wchar_t *value);

	// Sets the value of a date/time sub-element.
	void SetDate(const wchar_t *tag, SYSTEMTIME &dateTime);

	// Sets the value of a date/time sub-element to the current system date/time.
	void SetDateNow(const wchar_t *tag);

	// The same as SetDate, except the date/time is passed as an RFC822 string.
	void SetDateStr(const wchar_t *tag, const wchar_t *dateTimeStr);

	// Sets the value of an integer sub-element.
	void SetInt(const wchar_t *tag, int value);

	// Sets the value of a sub-element.
	void SetString(const wchar_t *tag, const wchar_t *value);

	// Returns the RSS document as an XML string.
	bool ToXmlString(CkString &outStr);
	// Returns the RSS document as an XML string.
	const wchar_t *toXmlString(void);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
