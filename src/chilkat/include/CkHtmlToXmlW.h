// CkHtmlToXmlW.h: interface for the CkHtmlToXmlW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkHtmlToXmlW_H
#define _CkHtmlToXmlW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkHtmlToXmlW
class CK_VISIBLE_PUBLIC CkHtmlToXmlW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkHtmlToXmlW(const CkHtmlToXmlW &);
	CkHtmlToXmlW &operator=(const CkHtmlToXmlW &);

    public:
	CkHtmlToXmlW(void);
	virtual ~CkHtmlToXmlW(void);

	static CkHtmlToXmlW *createNew(void);
	

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The HTML to be converted by the ToXml method. To convert HTML to XML, first set
	// this property to the HTML string and then call ToXml. The ConvertFile method can
	// do file-to-file conversions.
	void get_Html(CkString &str);
	// The HTML to be converted by the ToXml method. To convert HTML to XML, first set
	// this property to the HTML string and then call ToXml. The ConvertFile method can
	// do file-to-file conversions.
	const wchar_t *html(void);
	// The HTML to be converted by the ToXml method. To convert HTML to XML, first set
	// this property to the HTML string and then call ToXml. The ConvertFile method can
	// do file-to-file conversions.
	void put_Html(const wchar_t *newVal);

	// Determines how to handle   HTML entities. The default value, 0 will cause
	// _AMP_nbsp; entites to be convert to normal space characters (ASCII value 32). If
	// this property is set to 1, then _AMP_nbsp;'s will be converted to _AMP_#160. If
	// set to 2, then _AMP_nbps;'s are dropped. If set to 3, then _AMP_nbsp's are left
	// unmodified.
	int get_Nbsp(void);
	// Determines how to handle   HTML entities. The default value, 0 will cause
	// _AMP_nbsp; entites to be convert to normal space characters (ASCII value 32). If
	// this property is set to 1, then _AMP_nbsp;'s will be converted to _AMP_#160. If
	// set to 2, then _AMP_nbps;'s are dropped. If set to 3, then _AMP_nbsp's are left
	// unmodified.
	void put_Nbsp(int newVal);

	// The charset, such as "utf-8" or "iso-8859-1" of the XML to be created. If
	// XmlCharset is empty, the XML is created in the same character encoding as the
	// HTML. Otherwise the HTML is converted XML and converted to this charset.
	void get_XmlCharset(CkString &str);
	// The charset, such as "utf-8" or "iso-8859-1" of the XML to be created. If
	// XmlCharset is empty, the XML is created in the same character encoding as the
	// HTML. Otherwise the HTML is converted XML and converted to this charset.
	const wchar_t *xmlCharset(void);
	// The charset, such as "utf-8" or "iso-8859-1" of the XML to be created. If
	// XmlCharset is empty, the XML is created in the same character encoding as the
	// HTML. Otherwise the HTML is converted XML and converted to this charset.
	void put_XmlCharset(const wchar_t *newVal);

	// If set to true, then any non-standard HTML tags will be dropped when converting
	// to XML.
	bool get_DropCustomTags(void);
	// If set to true, then any non-standard HTML tags will be dropped when converting
	// to XML.
	void put_DropCustomTags(bool newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Converts an HTML file to a well-formed XML file that can be parsed for the
	// purpose of programmatically extracting information.
	bool ConvertFile(const wchar_t *inHtmlPath, const wchar_t *destXmlPath);

	// Allows for any specified tag to be dropped from the output XML. To drop more
	// than one tag, call this method once for each tag type to be dropped.
	void DropTagType(const wchar_t *tagName);

	// Causes text formatting tags to be dropped from the XML output. Text formatting
	// tags are: b, font, i, u, br, center, em, strong, big, tt, s, small, strike, sub,
	// and sup.
	void DropTextFormattingTags(void);

	// Returns true if the component is already unlocked. Otherwise returns false.
	bool IsUnlocked(void);

	// Convenience method for reading a text file into a string. The character encoding
	// of the text file is specified by  srcCharset. Valid values, such as "iso-8895-1" or
	// "utf-8" are listed at: List of Charsets
	// <http://blog.chilkatsoft.com/?p=463> .
	bool ReadFileToString(const wchar_t *filename, const wchar_t *srcCharset, CkString &outStr);
	// Convenience method for reading a text file into a string. The character encoding
	// of the text file is specified by  srcCharset. Valid values, such as "iso-8895-1" or
	// "utf-8" are listed at: List of Charsets
	// <http://blog.chilkatsoft.com/?p=463> .
	const wchar_t *readFileToString(const wchar_t *filename, const wchar_t *srcCharset);

	// Sets the Html property from a byte array.
	void SetHtmlBytes(const CkByteData &inData);

	// Sets the Html property by loading the HTML from a file.
	bool SetHtmlFromFile(const wchar_t *filename);

	// Converts the HTML in the "Html" property to XML and returns the XML string.
	bool ToXml(CkString &outStr);
	// Converts the HTML in the "Html" property to XML and returns the XML string.
	const wchar_t *toXml(void);

	// Causes a specified type of tag to NOT be dropped in the output XML.
	void UndropTagType(const wchar_t *tagName);

	// Causes text formatting tags to NOT be dropped from the XML output. Text
	// formatting tags are: b, font, i, u, br, center, em, strong, big, tt, s, small,
	// strike, sub, and sup.
	// 
	// Important: Text formatting tags are dropped by default. Call this method to
	// prevent text formatting tags from being dropped.
	// 
	void UndropTextFormattingTags(void);

	// Unlocks the component. An arbitrary unlock code may be passed to automatically
	// begin a 30-day trial.
	bool UnlockComponent(const wchar_t *unlockCode);

	// Convenience method for saving a string to a file. The character encoding of the
	// output text file is specified by  outpuCharset (the string is converted to this charset
	// when writing). Valid values, such as "iso-8895-1" or "utf-8" are listed at: List
	// of Charsets
	// <http://blog.chilkatsoft.com/?p=463> .
	bool WriteStringToFile(const wchar_t *str, const wchar_t *filename, const wchar_t *charset);

	// This is the same as the "ToXml" method. It converts the HTML in the "Html"
	// property to XML and returns the XML string.
	bool Xml(CkString &outStr);
	// This is the same as the "ToXml" method. It converts the HTML in the "Html"
	// property to XML and returns the XML string.
	const wchar_t *xml(void);

	// Convenience method for reading a complete file into a byte array.
	bool ReadFile(const wchar_t *path, CkByteData &outBytes);

	// Convenience method for saving a byte array to a file.
	bool WriteFile(const wchar_t *path, const CkByteData &fileData);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
