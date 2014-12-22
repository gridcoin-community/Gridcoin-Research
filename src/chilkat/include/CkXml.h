// CkXml.h: interface for the CkXml class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkXml_H
#define _CkXml_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkXml
class CK_VISIBLE_PUBLIC CkXml  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkXml(const CkXml &);
	CkXml &operator=(const CkXml &);

    public:
	CkXml(void);
	virtual ~CkXml(void);

	static CkXml *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// When True, causes an XML node's content to be encapsulated in a CDATA section.
	bool get_Cdata(void);
	// When True, causes an XML node's content to be encapsulated in a CDATA section.
	void put_Cdata(bool newVal);

	// The content of the XML node. It is the text between the open and close tags, not
	// including child nodes. For example:
	// _LT_tag1_GT_This is the content_LT_/tag1_GT_
	// 
	// _LT_tag2_GT__LT_child1_GT_abc_LT_/child1_GT__LT_child2_GT_abc_LT_/child2_GT_This is the content_LT_/tag2_GT_
	// Because the child nodes are not included, the content of "tag1" and "tag2" are
	// both equal to "This is the content".
	void get_Content(CkString &str);
	// The content of the XML node. It is the text between the open and close tags, not
	// including child nodes. For example:
	// _LT_tag1_GT_This is the content_LT_/tag1_GT_
	// 
	// _LT_tag2_GT__LT_child1_GT_abc_LT_/child1_GT__LT_child2_GT_abc_LT_/child2_GT_This is the content_LT_/tag2_GT_
	// Because the child nodes are not included, the content of "tag1" and "tag2" are
	// both equal to "This is the content".
	const char *content(void);
	// The content of the XML node. It is the text between the open and close tags, not
	// including child nodes. For example:
	// _LT_tag1_GT_This is the content_LT_/tag1_GT_
	// 
	// _LT_tag2_GT__LT_child1_GT_abc_LT_/child1_GT__LT_child2_GT_abc_LT_/child2_GT_This is the content_LT_/tag2_GT_
	// Because the child nodes are not included, the content of "tag1" and "tag2" are
	// both equal to "This is the content".
	void put_Content(const char *newVal);

	// Set/get the content as an integer.
	int get_ContentInt(void);
	// Set/get the content as an integer.
	void put_ContentInt(int newVal);

	// The DOCTYPE declaration (if any) for the XML document.
	void get_DocType(CkString &str);
	// The DOCTYPE declaration (if any) for the XML document.
	const char *docType(void);
	// The DOCTYPE declaration (if any) for the XML document.
	void put_DocType(const char *newVal);

	// If true, then the XML declaration is emitted for methods (such as GetXml or
	// SaveXml) where the XML is written to a file or string. The default value of this
	// property is true. If set to false, the XML declaration is not emitted. (The
	// XML declaration is the 1st line of an XML document starting with "_AMP_?xml
	// ...".
	bool get_EmitXmlDecl(void);
	// If true, then the XML declaration is emitted for methods (such as GetXml or
	// SaveXml) where the XML is written to a file or string. The default value of this
	// property is true. If set to false, the XML declaration is not emitted. (The
	// XML declaration is the 1st line of an XML document starting with "_AMP_?xml
	// ...".
	void put_EmitXmlDecl(bool newVal);

	// This is the encoding attribute in the XML declaration, such as "utf-8" or
	// "iso-8859-1". The default is "utf-8". This property can be set from any node in
	// the XML document and when set, causes the encoding property to be added to the
	// XML declaration. Setting this property does not cause the document to be
	// converted to a different encoding.
	// 
	// Calling any of the LoadXml* methods causes this property to be set to the
	// charset found within the XML, if any. If no charset is specified within the
	// loaded XML, then the LoadXml method resets this property to its default value of
	// "utf-8".
	// 
	void get_Encoding(CkString &str);
	// This is the encoding attribute in the XML declaration, such as "utf-8" or
	// "iso-8859-1". The default is "utf-8". This property can be set from any node in
	// the XML document and when set, causes the encoding property to be added to the
	// XML declaration. Setting this property does not cause the document to be
	// converted to a different encoding.
	// 
	// Calling any of the LoadXml* methods causes this property to be set to the
	// charset found within the XML, if any. If no charset is specified within the
	// loaded XML, then the LoadXml method resets this property to its default value of
	// "utf-8".
	// 
	const char *encoding(void);
	// This is the encoding attribute in the XML declaration, such as "utf-8" or
	// "iso-8859-1". The default is "utf-8". This property can be set from any node in
	// the XML document and when set, causes the encoding property to be added to the
	// XML declaration. Setting this property does not cause the document to be
	// converted to a different encoding.
	// 
	// Calling any of the LoadXml* methods causes this property to be set to the
	// charset found within the XML, if any. If no charset is specified within the
	// loaded XML, then the LoadXml method resets this property to its default value of
	// "utf-8".
	// 
	void put_Encoding(const char *newVal);

	// The number of attributes. For example, the following node has 2 attributes:
	// _LT_tag attr1="value1" attr2="value2"> This is the content_LT_/tag>
	int get_NumAttributes(void);

	// The number of direct child nodes contained under this XML node.
	int get_NumChildren(void);

	// If true (or 1 for ActiveX), then all Sort* methods use case insensitive sorting.
	bool get_SortCaseInsensitive(void);
	// If true (or 1 for ActiveX), then all Sort* methods use case insensitive sorting.
	void put_SortCaseInsensitive(bool newVal);

	// This is the standalone attribute in the XML declaration. This property can be
	// set from any node in the XML document. A value of true adds a standalone="yes"
	// to the XML declaration:
	// _LT_?xml ... standalone="yes">
	bool get_Standalone(void);
	// This is the standalone attribute in the XML declaration. This property can be
	// set from any node in the XML document. A value of true adds a standalone="yes"
	// to the XML declaration:
	// _LT_?xml ... standalone="yes">
	void put_Standalone(bool newVal);

	// The XML node's tag.
	void get_Tag(CkString &str);
	// The XML node's tag.
	const char *tag(void);
	// The XML node's tag.
	void put_Tag(const char *newVal);

	// Each tree (or XML document) has a unique TreeId. This is the ID of the tree, and
	// can be used to determine if two Xml objects belong to the same tree.
	int get_TreeId(void);



	// ----------------------
	// Methods
	// ----------------------
	// Accumulates the content of all nodes having a specific tag into a single result
	// string. SkipTags specifies a set of tags whose are to be avoided. The skipTags
	// are formatted as a string of tags delimited by vertical bar characters. All
	// nodes in sub-trees rooted with a tag appearing in skipTags are not included in
	// the result.
	bool AccumulateTagContent(const char *tag, const char *skipTags, CkString &outStr);
	// Accumulates the content of all nodes having a specific tag into a single result
	// string. SkipTags specifies a set of tags whose are to be avoided. The skipTags
	// are formatted as a string of tags delimited by vertical bar characters. All
	// nodes in sub-trees rooted with a tag appearing in skipTags are not included in
	// the result.
	const char *accumulateTagContent(const char *tag, const char *skipTags);

	// Adds an attribute to the calling node in the XML document. Returns True for
	// success, and False for failure.
	bool AddAttribute(const char *name, const char *value);

	// Adds an integer attribute to a node.
	bool AddAttributeInt(const char *name, int value);

	// Adds an entire subtree as a child. If the child was a subtree within another Xml
	// document then the subtree is effectively transferred from one XML document to
	// another.
	bool AddChildTree(const CkXml &tree);

	// Adds an attribute to an XML node. If an attribute having the specified name
	// already exists, the value is updated.
	void AddOrUpdateAttribute(const char *name, const char *value);

	// Adds an integer attribute to an XML node. If an attribute having the specified
	// name already exists, the value is updated.
	void AddOrUpdateAttributeI(const char *name, int value);

	// Adds a style sheet declaration to the XML document. The styleSheet should be a string
	// such as:
	// _LT_?xml-stylesheet href="mystyle.css" title="Compact" type="text/css"?>
	void AddStyleSheet(const char *styleSheet);

	// Adds an integer amount to an integer attribute's value. If the attribute does
	// not yet exist, this method behaves the same as AddOrUpdateAttributeI.
	void AddToAttribute(const char *name, int amount);

	// Adds an integer value to the content of a child node.
	void AddToChildContent(const char *tag, int amount);

	// Adds an integer amount to the node's content.
	void AddToContent(int amount);

	// Appends text to the content of an XML node
	bool AppendToContent(const char *str);

	// Sets the node's content with 8bit data that is in a specified multibyte
	// character encoding such as utf-8, shift-jis, big5, etc. The data is first
	// B-encoded and the content is set to be the B-encoded string. For example, if
	// called with "Big5"for the charset, you would get a string that looks something
	// like this: "=?Big5?B?pHCtsw==?=". The data is Base64-encoded and stored between
	// the last pair of "?" delimiters. Use the DecodeContent method to retrieve the
	// byte data from a B encoded string.
	bool BEncodeContent(const char *charset, const CkByteData &inData);

	// Return true if a child having a specific tag contains content that matches a
	// wildcarded pattern.
	bool ChildContentMatches(const char *tag, const char *pattern, bool caseSensitive);

	// Follows a series of commands to navigate through an XML document to return a
	// piece of data or update the caller's reference to a new XML document node.
	// 
	// Note: This method not related to the XPath (XML Path) standard in any way.
	// 
	// The pathCmd is formatted as a series of commands separated by vertical bar
	// characters, and terminated with a return-command:
	//     command|command|command|...|returnCommand
	// 
	// A command can be any of the following:
	//     TagName -- Navigate to the 1st direct child with the given tag.
	//     TagName[n] -- Navigate to the Nth direct child with the given tag.
	//     .. -- Navigate up to the parent
	//     TagName{Content} -- Navigate to the 1st direct child with the given tag
	//     having the exact content.
	//     /T/TagName -- Traverse the XML DOM tree (rooted at the caller) and navigate
	//     to the 1st node having the given tag.
	//     /C/TagName,ContentPattern -- Traverse the XML DOM tree (rooted at the
	//     caller) and navigate to the 1st node having the given tag with content that
	//     matches the ContentPattern. ContentPattern may use one or more asterisk ('*")
	//     characters to represent 0 or more of any character.
	//     /C/ContentPattern -- Traverse the XML DOM tree (rooted at the caller) and
	//     navigate to the 1st node having any tag with content that matches the
	//     ContentPattern. ContentPattern may use one or more asterisk ('*") characters to
	//     represent 0 or more of any character.
	//     /A/TagName,AttrName,AttrValuePattern -- Traverse the XML DOM tree (rooted at
	//     the caller) and navigate to the 1st node having the given tag, and attribute,
	//     with the attribute value that matches the AttrValuePattern. AttrValuePattern may
	//     use one or more asterisk ('*") characters to represent 0 or more of any
	//     character.
	// The returnCommand can be any of the following:
	//     * -- Return the Content of the node.
	//     (AttrName) -- Return the value of the given attribute.
	//     $ -- Update the caller's internal reference to be the node (arrived at by
	//     following the series of commands). Returns an empty string.
	// 
	bool ChilkatPath(const char *pathCmd, CkString &outStr);
	// Follows a series of commands to navigate through an XML document to return a
	// piece of data or update the caller's reference to a new XML document node.
	// 
	// Note: This method not related to the XPath (XML Path) standard in any way.
	// 
	// The pathCmd is formatted as a series of commands separated by vertical bar
	// characters, and terminated with a return-command:
	//     command|command|command|...|returnCommand
	// 
	// A command can be any of the following:
	//     TagName -- Navigate to the 1st direct child with the given tag.
	//     TagName[n] -- Navigate to the Nth direct child with the given tag.
	//     .. -- Navigate up to the parent
	//     TagName{Content} -- Navigate to the 1st direct child with the given tag
	//     having the exact content.
	//     /T/TagName -- Traverse the XML DOM tree (rooted at the caller) and navigate
	//     to the 1st node having the given tag.
	//     /C/TagName,ContentPattern -- Traverse the XML DOM tree (rooted at the
	//     caller) and navigate to the 1st node having the given tag with content that
	//     matches the ContentPattern. ContentPattern may use one or more asterisk ('*")
	//     characters to represent 0 or more of any character.
	//     /C/ContentPattern -- Traverse the XML DOM tree (rooted at the caller) and
	//     navigate to the 1st node having any tag with content that matches the
	//     ContentPattern. ContentPattern may use one or more asterisk ('*") characters to
	//     represent 0 or more of any character.
	//     /A/TagName,AttrName,AttrValuePattern -- Traverse the XML DOM tree (rooted at
	//     the caller) and navigate to the 1st node having the given tag, and attribute,
	//     with the attribute value that matches the AttrValuePattern. AttrValuePattern may
	//     use one or more asterisk ('*") characters to represent 0 or more of any
	//     character.
	// The returnCommand can be any of the following:
	//     * -- Return the Content of the node.
	//     (AttrName) -- Return the value of the given attribute.
	//     $ -- Update the caller's internal reference to be the node (arrived at by
	//     following the series of commands). Returns an empty string.
	// 
	const char *chilkatPath(const char *pathCmd);

	// Removes all children, attributes, and content from the XML node. Resets the tag
	// name to "unnamed".
	bool Clear(void);

	// Return true if the node's content matches a wildcarded pattern.
	bool ContentMatches(const char *pattern, bool caseSensitive);

	// Copies the tag, content, and attributes to the calling node.
	void Copy(const CkXml &node);

	// Discards the caller's current internal reference and copies the internal
	// reference from copyFromNode. Effectively updates the caller node to point to the same
	// node in the XML document as copyFromNode.
	void CopyRef(CkXml &node);

	// Decodes a node's Q or B-encoded content string and returns the byte data.
	bool DecodeContent(CkByteData &outData);

	// Utility method to decode HTML entities. It accepts a string containing
	// (potentially) HTML entities and returns a string with the entities decoded.
	bool DecodeEntities(const char *str, CkString &outStr);
	// Utility method to decode HTML entities. It accepts a string containing
	// (potentially) HTML entities and returns a string with the entities decoded.
	const char *decodeEntities(const char *str);

	// Decrypts the content of an XML node that was previously 128-bit AES encrypted
	// with the EncryptContent method.
	bool DecryptContent(const char *password);

	// Encrypts the content of the calling XML node using 128-bit CBC AES encryption.
	// The base64-encoded encrypted content replaces the original content.
	bool EncryptContent(const char *password);

	// Removes and returns the Nth child of an XML node. The first child is at index 0.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *ExtractChildByIndex(int index);

	// Removes and returns the first child node having a tag equal to the tagName. The
	// attributeName and attrValue may be empty or NULL, in which case the first child
	// matching the tag is removed and returned. If attributeName is specified, then
	// the first child having a tag equal to tagName, and an attribute with
	// attributeName is returned. If attrValue is also specified, then only a child
	// having a tag equal to tagName, and an attribute named attributeName, with a
	// value equal to attrValue is returned.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *ExtractChildByName(const char *tag, const char *attrName, const char *attrValue);

	// Returns the child having a specified tag.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *FindChild(const char *tag);

	// Updates the Xml object's internal reference to point to a child with a specified
	// tag.
	bool FindChild2(const char *tag);

	// Returns the next record node where the child with a specific tag matches a
	// wildcarded pattern. This method makes it easy to iterate over high-level
	// records.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *FindNextRecord(const char *tag, const char *contentPattern);

	// First searches for a child having a tag equal to tagName, and if found, returns
	// it. Otherwise creates a new child, sets the tag equal to tagName, and
	// initializes the Content to empty.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *FindOrAddNewChild(const char *tag);

	// Returns the first child. A program can step through the children by calling
	// FirstChild, and then NextSibling repeatedly.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *FirstChild(void);

	// Updates the internal reference of the caller to point to its first child.
	bool FirstChild2(void);

	// Find and return the value of an attribute having a specified name.
	bool GetAttrValue(const char *name, CkString &outStr);
	// Find and return the value of an attribute having a specified name.
	const char *getAttrValue(const char *name);
	// Find and return the value of an attribute having a specified name.
	const char *attrValue(const char *name);

	// Returns an attribute as an integer.
	int GetAttrValueInt(const char *name);

	// Returns the name of the Nth attribute of an XML node. The first attribute is at
	// index 0.
	bool GetAttributeName(int index, CkString &outStr);
	// Returns the name of the Nth attribute of an XML node. The first attribute is at
	// index 0.
	const char *getAttributeName(int index);
	// Returns the name of the Nth attribute of an XML node. The first attribute is at
	// index 0.
	const char *attributeName(int index);

	// Returns the value of the Nth attribute of an XML node. The first attribute is at
	// index 0.
	bool GetAttributeValue(int index, CkString &outStr);
	// Returns the value of the Nth attribute of an XML node. The first attribute is at
	// index 0.
	const char *getAttributeValue(int index);
	// Returns the value of the Nth attribute of an XML node. The first attribute is at
	// index 0.
	const char *attributeValue(int index);

	// Returns an attribute as an integer.
	int GetAttributeValueInt(int index);

	// Returns binary content of an XML node as a byte array. The content may have been
	// Zip compressed, AES encrypted, or both. Unzip compression and AES decryption
	// flags should be set appropriately.
	bool GetBinaryContent(bool unzipFlag, bool decryptFlag, const char *password, CkByteData &outData);

	// Returns the Nth child of an XML node
	// The caller is responsible for deleting the object returned by this method.
	CkXml *GetChild(int index);

	// Updates the calling object's internal reference to the Nth child node.
	bool GetChild2(int index);

	// Returns false if the node's content is "0", otherwise returns true if the
	// node contains a non-zero integer.
	bool GetChildBoolValue(const char *tag);

	// Returns the content of a child having a specified tag.
	bool GetChildContent(const char *tag, CkString &outStr);
	// Returns the content of a child having a specified tag.
	const char *getChildContent(const char *tag);
	// Returns the content of a child having a specified tag.
	const char *childContent(const char *tag);

	// Returns the content of the Nth child node.
	bool GetChildContentByIndex(int index, CkString &outStr);
	// Returns the content of the Nth child node.
	const char *getChildContentByIndex(int index);
	// Returns the content of the Nth child node.
	const char *childContentByIndex(int index);

	// Returns the child having the exact tag and content.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *GetChildExact(const char *tag, const char *content);

	// Returns the child integer content for a given tag.
	int GetChildIntValue(const char *tag);

	// Returns the tag name of the Nth child node.
	bool GetChildTag(int index, CkString &outStr);
	// Returns the tag name of the Nth child node.
	const char *getChildTag(int index);
	// Returns the tag name of the Nth child node.
	const char *childTag(int index);

	// Returns the tag name of the Nth child node.
	bool GetChildTagByIndex(int index, CkString &outStr);
	// Returns the tag name of the Nth child node.
	const char *getChildTagByIndex(int index);
	// Returns the tag name of the Nth child node.
	const char *childTagByIndex(int index);

	// Finds and returns the XML child node having both a given tag and an attribute
	// with a given name and value.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *GetChildWithAttr(const char *tag, const char *attrName, const char *attrValue);

	// Returns the first child found having the exact content specified.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *GetChildWithContent(const char *content);

	// Returns the Xml child object having a tag matching tagName.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *GetChildWithTag(const char *tag);

	// Returns the Nth child having a tag that matches exactly with the tagName. Use
	// the NumChildrenHavingTag method to determine how many children have a particular
	// tag.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *GetNthChildWithTag(const char *tag, int n);

	// Updates the calling object's internal reference to the Nth child node having a
	// specific tag.
	bool GetNthChildWithTag2(const char *tag, int n);

	// Returns the parent of this XML node, or NULL if the node is the root of the
	// tree.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *GetParent(void);

	// Updates the internal reference of the caller to its parent.
	bool GetParent2(void);

	// Returns the root node of the XML document
	// The caller is responsible for deleting the object returned by this method.
	CkXml *GetRoot(void);

	// Updates the internal reference of the caller to the document root.
	void GetRoot2(void);

	// Returns a new XML object instance that references the same XML node.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *GetSelf(void);

	// Generate the XML text document for the XML tree rooted at this node. If called
	// from the root node of the XML document, then the XML declarator ("_LT_?xml
	// version="1.0" encoding="utf-8" ?>") is included at the beginning of the XML.
	// Otherwise, it is not included.
	bool GetXml(CkString &outStr);
	// Generate the XML text document for the XML tree rooted at this node. If called
	// from the root node of the XML document, then the XML declarator ("_LT_?xml
	// version="1.0" encoding="utf-8" ?>") is included at the beginning of the XML.
	// Otherwise, it is not included.
	const char *getXml(void);
	// Generate the XML text document for the XML tree rooted at this node. If called
	// from the root node of the XML document, then the XML declarator ("_LT_?xml
	// version="1.0" encoding="utf-8" ?>") is included at the beginning of the XML.
	// Otherwise, it is not included.
	const char *xml(void);

	// Returns true if the node contains attribute with the name and value.
	bool HasAttrWithValue(const char *name, const char *value);

	// Returns true if the node contains an attribute with the specified name.
	bool HasAttribute(const char *name);

	// Returns true if the node has a direct child node containing the exact content
	// string specified.
	bool HasChildWithContent(const char *content);

	// Returns true (1 for ActiveX) if the node has a direct child with a given tag.
	bool HasChildWithTag(const char *tag);

	// Returns true if the node contains a direct child having the exact tag and
	// content specified.
	bool HasChildWithTagAndContent(const char *tag, const char *content);

	// Adds an entire subtree as a child. If the child was a subtree within another Xml
	// document then the subtree is effectively transferred from one XML document to
	// another. The child tree is inserted in a position after the Nth child (of the
	// calling node).
	bool InsertChildTreeAfter(int index, const CkXml &tree);

	// Adds an entire subtree as a child. If the child was a subtree within another Xml
	// document then the subtree is effectively transferred from one XML document to
	// another. The child tree is inserted in a position before the Nth child (of the
	// calling node).
	bool InsertChildTreeBefore(int index, const CkXml &tree);

	// Returns the last Xml child node. A node's children can be enumerated by calling
	// LastChild and then repeatedly calling PreviousSibling, until a NULL is returned.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *LastChild(void);

	// Updates the internal reference of the caller to its last child.
	bool LastChild2(void);

	// Loads an XML document from a memory buffer and returns true if successful. The
	// contents of the calling node are replaced with the root node of the XML document
	// loaded.
	bool LoadXml(const char *xmlData);

	// Same as LoadXml, but an additional argument controls whether or not
	// leading/trailing whitespace is auto-trimmed from each node's content.
	bool LoadXml2(const char *xmlData, bool autoTrim);

	// Loads an XML document from a file and returns true if successful. The contents
	// of the calling node are replaced with the root node of the XML document loaded.
	bool LoadXmlFile(const char *fileName);

	// Same as LoadXmlFile, but an additional argument controls whether or not
	// leading/trailing whitespace is auto-trimmed from each node's content.
	bool LoadXmlFile2(const char *fileName, bool autoTrim);

	// Creates a new child having tag and content. The new child is created even if a
	// child with a tag equal to tagName already exists. (Use FindOrAddNewChild to
	// prevent creating children having the same tags.)
	// The caller is responsible for deleting the object returned by this method.
	CkXml *NewChild(const char *tag, const char *content);

	// Creates a new child but does not return the node that is created.
	void NewChild2(const char *tag, const char *content);

	// Inserts a new child in a position after the Nth child node.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *NewChildAfter(int index, const char *tag, const char *content);

	// Inserts a new child in a position before the Nth child node.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *NewChildBefore(int index, const char *tag, const char *content);

	// Inserts a new child having an integer for content.
	void NewChildInt2(const char *tag, int value);

	// Returns the nodes next sibling, or NULL if there are no more.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *NextSibling(void);

	// Updates the internal reference of the caller to its next sibling.
	bool NextSibling2(void);

	// Returns the number of children having a specific tag name.
	int NumChildrenHavingTag(const char *tag);

	// Returns the Xml object that is the node's previous sibling, or NULL if there are
	// no more.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *PreviousSibling(void);

	// Updates the internal reference of the caller to its previous sibling.
	bool PreviousSibling2(void);

	// Sets the node's content with 8bit data that is in a specified multibyte
	// character encoding such as utf-8, shift-jis, big5, etc. The data is first
	// Q-encoded and the content is set to be the Q-encoded string. For example, if
	// called with "gb2312"for the charset, you would get a string that looks something
	// like this: "=?gb2312?Q?=C5=B5=BB=F9?=". Character that are not 7bit are
	// represented as "=XX" where XX is the hexidecimal value of the byte. Use the
	// DecodeContent method to retrieve the byte data from a Q encoded string.
	bool QEncodeContent(const char *charset, const CkByteData &inData);

	// Removes all attributes from an XML node. Should always return True.
	bool RemoveAllAttributes(void);

	// Removes all children from the calling node.
	void RemoveAllChildren(void);

	// Removes an attribute by name from and XML node.
	bool RemoveAttribute(const char *name);

	// Removes all direct children with a given tag.
	void RemoveChild(const char *tag);

	// Removes the Nth child from the calling node.
	void RemoveChildByIndex(int index);

	// Removes all children having the exact content specified.
	void RemoveChildWithContent(const char *content);

	// Removes the calling object and its sub-tree from the XML document making it the
	// root of its own tree.
	void RemoveFromTree(void);

	// Saves a node's binary content to a file.
	bool SaveBinaryContent(const char *filename, bool unzipFlag, bool decryptFlag, const char *password);

	// Generates XML representing the tree or subtree rooted at this node and writes it
	// to a file.
	bool SaveXml(const char *fileName);

	// Returns the first node whose content matches the contentPattern, which is a
	// case-sensitive string that can use any number of '*'s to represent 0 or more
	// occurances of any character. The search is breadth-first over the sub-tree
	// rooted at the caller, and the previous node returned can be passed to the next
	// call as afterNode to continue the search after that node.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *SearchAllForContent(const CkXml *afterPtr, const char *contentPattern);

	// Same as SearchAllForContent except the internal reference of the caller is
	// updated to point to the search result (instead of returning a new object).
	bool SearchAllForContent2(const CkXml *afterPtr, const char *contentPattern);

	// Returns the first node having a tag equal to tagName, and an attribute named
	// attrName whose value matches valuePattern, which is a case-sensitive string that
	// can use any number of '*'s to represent 0 or more occurances of any character.
	// The search is breadth-first over the sub-tree rooted at the caller, and the
	// previous node returned can be passed to the next call as afterNode to continue
	// the search after that node.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *SearchForAttribute(const CkXml *afterPtr, const char *tag, const char *attr, const char *valuePattern);

	// Same as SearchForAttribute except the internal reference of the caller is
	// updated to point to the search result (instead of returning a new object).
	bool SearchForAttribute2(const CkXml *afterPtr, const char *tag, const char *attr, const char *valuePattern);

	// Returns the first node having a tag equal to tagName whose content matches
	// contentPattern, which is a case-sensitive string that can use any number of '*'s
	// to represent 0 or more occurances of any character. The search is breadth-first
	// over the sub-tree rooted at the caller, and the previous node returned can be
	// passed to the next call as afterNode to continue the search after that node.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *SearchForContent(const CkXml *afterPtr, const char *tag, const char *contentPattern);

	// Same as SearchForContent except the internal reference of the caller is updated
	// to point to the search result (instead of returning a new object).
	bool SearchForContent2(const CkXml *afterPtr, const char *tag, const char *contentPattern);

	// Returns the first node having a tag equal to tagName. The search is
	// breadth-first over the sub-tree rooted at the caller, and the previous node
	// returned can be passed to the next call as afterNode to continue the search
	// after that node.
	// The caller is responsible for deleting the object returned by this method.
	CkXml *SearchForTag(const CkXml *afterPtr, const char *tag);

	// Same as SearchForTag except the internal reference of the caller is updated to
	// point to the search result (instead of returning a new object).
	bool SearchForTag2(const CkXml *afterPtr, const char *tag);

	// Sets the node's content to a block of binary data with optional Zip compression
	// and/or AES encryption. The binary data is automatically converted to base64
	// format whenever XML text is generated. If the zipFlag is True, the data is first
	// compressed. If the encryptFlag is True, the data is AES encrypted using the
	// Rijndael 128-bit symmetric-encryption algorithm.
	bool SetBinaryContent(const CkByteData &inData, bool zipFlag, bool encryptFlag, const char *password);

#if !defined(CHILKAT_MONO)
	// The same as SetBinaryContent but the data is provided via a pointer and byte
	// count.
	bool SetBinaryContent2(const unsigned char *pByteData, unsigned long szByteData, bool zipFlag, bool encryptFlag, const char *password);
#endif

	// Sets the node's content with binary (or text) data from a file. The file
	// contents can be Zip compressed and/or encrypted, and the result is base-64
	// encoded.
	bool SetBinaryContentFromFile(const char *filename, bool zipFlag, bool encryptFlag, const char *password);

	// Sorts the direct child nodes by the value of a specified attribute.
	void SortByAttribute(const char *attrName, bool ascending);

	// Sorts the direct child nodes by the value of a specified attribute interpreted
	// as an integer (not lexicographically as strings).
	void SortByAttributeInt(const char *attrName, bool ascending);

	// Sorts the direct child nodes by content.
	void SortByContent(bool ascending);

	// Sorts the direct child nodes by tag.
	void SortByTag(bool ascending);

	// Sorts the direct child nodes by the content of an attribute in the grandchild
	// nodes.
	void SortRecordsByAttribute(const char *sortTag, const char *attrName, bool ascending);

	// Sorts the direct child nodes by the content of the grandchild nodes.
	void SortRecordsByContent(const char *sortTag, bool ascending);

	// Sorts the direct child nodes by the content of the grandchild nodes. For sorting
	// purposes, the content is interpreted as an integer (not lexicographically as for
	// strings).
	void SortRecordsByContentInt(const char *sortTag, bool ascending);

	// Swaps another node's tag, content, and attributes with this one.
	bool SwapNode(const CkXml &node);

	// Swaps another node's tag, content, attributes, and children with this one.
	bool SwapTree(const CkXml &tree);

	// Returns the content of the 1st node found in the sub-tree rooted at the caller
	// that has a given tag. (Note: The search for the node having tag ARG is not
	// limited to the direct children of the caller.)
	bool TagContent(const char *tag, CkString &outStr);
	// Returns the content of the 1st node found in the sub-tree rooted at the caller
	// that has a given tag. (Note: The search for the node having tag ARG is not
	// limited to the direct children of the caller.)
	const char *tagContent(const char *tag);

	// Returns true if the node's tag equals the specified string.
	bool TagEquals(const char *tag);

	// Unzip the content of the XML node replacing it's content with the decompressed
	// data.
	bool UnzipContent(void);

	// Unzips and recreates the XML node and the entire subtree, restoring it to the
	// state before it was zip compressed.
	bool UnzipTree(void);

	// Adds an attribute to the node if it doesn't already exist. Otherwise it updates
	// the existing attribute with the new value.
	bool UpdateAttribute(const char *attrName, const char *attrValue);

	// Updates an attribute value. (Call UpdateAttribute if the attribute value is a
	// string.)
	bool UpdateAttributeInt(const char *attrName, int value);

	// Replaces the content of a child node.
	void UpdateChildContent(const char *tag, const char *value);

	// Replaces the content of a child node where the content is an integer.
	void UpdateChildContentInt(const char *tag, int value);

	// Applies Zip compression to the content of an XML node and replaces the content
	// with base64-encoded compressed data.
	bool ZipContent(void);

	// Zip compresses the content and entire subtree rooted at the calling XML node and
	// replaces the current content with base64-encoded Zip compressed data. The node
	// and subtree can be restored by calling UnzipTree. Note that the node name and
	// attributes are unaffected.
	bool ZipTree(void);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
