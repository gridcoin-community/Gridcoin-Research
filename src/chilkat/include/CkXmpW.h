// CkXmpW.h: interface for the CkXmpW class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkXmpW_H
#define _CkXmpW_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkWideCharBase.h"

class CkXmlW;
class CkStringArrayW;
class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkXmpW
class CK_VISIBLE_PUBLIC CkXmpW  : public CkWideCharBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkXmpW(const CkXmpW &);
	CkXmpW &operator=(const CkXmpW &);

    public:
	CkXmpW(void);
	virtual ~CkXmpW(void);

	static CkXmpW *createNew(void);
	

	
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// Determines whether structures are stored with rdf:parseType="Resource", or
	// within an "rdf:Description" sub-node.
	bool get_StructInnerDescrip(void);
	// Determines whether structures are stored with rdf:parseType="Resource", or
	// within an "rdf:Description" sub-node.
	void put_StructInnerDescrip(bool newVal);

	// The number of XMP metadata documents found within the JPG or TIFF file loaded by
	// LoadAppFile.
	int get_NumEmbedded(void);



	// ----------------------
	// Methods
	// ----------------------
	// Adds or replaces an XMP property array. The XMP metadata to be updated is
	// contained in the XML object passed in the 1st argument. The 2nd argument
	// specifies the array type, which can be "bag", "seq", or "alt". The property name
	// should be prefixed with the namespace, such as "dc:subject".
	bool AddArray(CkXmlW &xml, const wchar_t *arrType, const wchar_t *propName, CkStringArrayW &values);

	// Adds a namespace to URI mapping. When a property is added via AddSimpleString or
	// any of the other methods, the property name is namespace qualified. When adding
	// the first property in a namespace, the rdf:Description is automatically added
	// and the URI is obtained from the namespace-to-URI mappings. The standard (and
	// commonly used) namespace mappings are defined by default. This is only used if
	// the namespace is custom or not already handled.
	void AddNsMapping(const wchar_t *ns, const wchar_t *uri);

	// Adds or replaces an XMP date property. The XMP metadata to be updated is
	// contained in the XML object passed in the 1st argument. The property name should
	// be prefixed with the namespace, such as "xap:CreateDate".
	bool AddSimpleDate(CkXmlW &iXml, const wchar_t *propName, SYSTEMTIME &propVal);

	// Adds or updates an XMP integer property. The XMP metadata to be updated is
	// contained in the XML object passed in the 1st argument. The property name should
	// be prefixed with the namespace, such as "tiff:XResolution".
	bool AddSimpleInt(CkXmlW &iXml, const wchar_t *propName, int propVal);

	// Adds or updates a simple XMP string property. The XMP metadata to be updated is
	// contained in the XML object passed in the 1st argument. The property name should
	// be prefixed with the namespace, such as "photoshop:Credit".
	bool AddSimpleStr(CkXmlW &iXml, const wchar_t *propName, const wchar_t *propVal);

	// Adds or updates an XMP structured property value. The XMP metadata to be updated
	// is contained in the XML object passed in the 1st argument. The structure name
	// should be prefixed with the namespace, such as
	// "Iptc4xmpCore:CreatorContactInfo". The property name within the structure should
	// also be prefixed with the namespace, such as "Iptc4xmpCore:CiAdrCity".
	bool AddStructProp(CkXmlW &iChilkatXml, const wchar_t *structName, const wchar_t *propName, const wchar_t *propValue);

	// Appends a new XMP metadata file to the XMP object. Any XMPs appended via this
	// method will be present in the file when SaveAppFile is called. Files containing
	// XMP metadata typically only include a single XMP document, so this method is
	// usually only called when adding XMP metadata to a file for the first time.
	bool Append(CkXmlW &iXml);

	// Converts a date to a string representation. This method is added for convenience
	// (just in case date-to-string conversions are required).
	bool DateToString(SYSTEMTIME &d, CkString &outStr);
	// Converts a date to a string representation. This method is added for convenience
	// (just in case date-to-string conversions are required).
	const wchar_t *dateToString(SYSTEMTIME &d);

	// Finds and returns an XMP array property. The property name should be prefixed
	// with the namespace, such as "dc:subject".
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *GetArray(CkXmlW &iXml, const wchar_t *propName);

	// Returns the Nth embedded XMP document as a Chilkat XML object.
	// The caller is responsible for deleting the object returned by this method.
	CkXmlW *GetEmbedded(int index);


	// The caller is responsible for deleting the object returned by this method.
	CkXmlW *GetProperty(CkXmlW &iXml, const wchar_t *propName);

	// Finds and returns an XMP date property. The property name should be prefixed
	// with the namespace, such as "xap:ModifyDate".
	bool GetSimpleDate(CkXmlW &iXml, const wchar_t *propName, SYSTEMTIME &outSysTime);

	// Finds and returns an XMP integer property. The property name should be prefixed
	// with the namespace, such as "tiff:ResolutionUnit".
	int GetSimpleInt(CkXmlW &iXml, const wchar_t *propName);

	// Finds and returns an XMP simple string property. The property name should be
	// prefixed with the namespace, such as "photoshop:Source".
	bool GetSimpleStr(CkXmlW &iXml, const wchar_t *propName, CkString &outStr);
	// Finds and returns an XMP simple string property. The property name should be
	// prefixed with the namespace, such as "photoshop:Source".
	const wchar_t *getSimpleStr(CkXmlW &iXml, const wchar_t *propName);
	// Finds and returns an XMP simple string property. The property name should be
	// prefixed with the namespace, such as "photoshop:Source".
	const wchar_t *simpleStr(CkXmlW &iXml, const wchar_t *propName);

	// Returns the property names used by an exsting structure within an XMP document.
	// The contents of the structure can be retrieved by calling GetStructValue for
	// each property name returned by GetStructPropNames.
	// The caller is responsible for deleting the object returned by this method.
	CkStringArrayW *GetStructPropNames(CkXmlW &iXml, const wchar_t *structName);

	// Returns the value of a single item within an XMP structure property. Property
	// names should always be prefixed with the namespace.
	bool GetStructValue(CkXmlW &iXml, const wchar_t *structName, const wchar_t *propName, CkString &outStr);
	// Returns the value of a single item within an XMP structure property. Property
	// names should always be prefixed with the namespace.
	const wchar_t *getStructValue(CkXmlW &iXml, const wchar_t *structName, const wchar_t *propName);
	// Returns the value of a single item within an XMP structure property. Property
	// names should always be prefixed with the namespace.
	const wchar_t *structValue(CkXmlW &iXml, const wchar_t *structName, const wchar_t *propName);

	// Loads a TIFF or JPG file into the XMP object.
	bool LoadAppFile(const wchar_t *filename);

	// Loads a JPG or TIFF from an byte buffer containing the image file data.
	bool LoadFromBuffer(const CkByteData &fileData, const wchar_t *ext);

	// Creates and returns a new/empty XMP metadata document as a Chilkat XML object.
	// The caller is responsible for deleting the object returned by this method.
	CkXmlW *NewXmp(void);

	// Removes all XMP metadata documents from an XMP object. After calling this
	// method, call SaveAppFile to rewrite the JPG or TIFF file with the XMP metadata
	// removed.
	bool RemoveAllEmbedded(void);

	// Removes an XMP array property from the XMP document.
	bool RemoveArray(CkXmlW &iXml, const wchar_t *propName);

	// Removes a single XMP metadata document from the JPG or TIFF file. Call
	// SaveAppFile to persist the changes to disk.
	bool RemoveEmbedded(int index);

	// Removes a namespace-to-URI mapping.
	void RemoveNsMapping(const wchar_t *ns);

	// Removes a simple XMP property from the XMP document.
	bool RemoveSimple(CkXmlW &iXml, const wchar_t *propName);

	// Removes an XMP structure property from the XMP document.
	bool RemoveStruct(CkXmlW &iXml, const wchar_t *structName);

	// Removes a single member from an XMP structured property.
	bool RemoveStructProp(CkXmlW &iXml, const wchar_t *structName, const wchar_t *propName);

	// Persists all changes made to the XMP document(s) by saving the XMP object to a
	// file. Changes made by adding, updating, or removing properties are not persisted
	// to the filesystem until this is called.
	bool SaveAppFile(const wchar_t *filename);

	// Saves a JPG or TIFF image with updated XMP to a byte buffer.
	bool SaveToBuffer(CkByteData &outBytes);

	// Converts a string to a date value. This method is added for convenience (just in
	// case string-to-date conversions are required).
	bool StringToDate(const wchar_t *str, SYSTEMTIME &outSysTime);

	// Unlocks the XMP component at runtime. This must be called once at the beginning
	// of your application. Passing an arbitrary value initiates a fully-functional
	// 30-day trial. A permanent unlock code is required to use the component beyond 30
	// days.
	bool UnlockComponent(const wchar_t *unlockCode);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
