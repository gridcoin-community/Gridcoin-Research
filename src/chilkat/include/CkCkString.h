// CkCkString.h: interface for the CkCkString class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated.

#ifndef _CkCkString_H
#define _CkCkString_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkString;
class CkStringArray;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkCkString
class CkCkString  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkCkString(const CkCkString &);
	CkCkString &operator=(const CkCkString &);

    public:
	CkCkString(void);
	virtual ~CkCkString(void);

	static CkCkString *createNew(void);
	void inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------


	// ----------------------
	// Methods
	// ----------------------
	void append(const char *s);

	void appendAnsi(const char *s);

	void appendChar(char c);

	void appendCurrentDateRfc822(void);

	void appendDateRfc822(SYSTEMTIME &sysTime);

	void appendDateRfc822Gmt(SYSTEMTIME &sysTime);

	void appendEnc(const char *s, const char *charEncoding);

	void appendHexData(const unsigned char *pByteData, unsigned long szByteData);

	void appendInt(int n);

#ifdef WIN32
	void appendLastWindowsError(void);
#endif

	void appendN(const char *pByteData, unsigned long szByteData);

	void appendNU(const wchar_t *unicode, int numChars);

	void appendRandom(int numBytes, const char *encoding);

	void appendStr(const CkString &str);

	void appendU(const wchar_t *unicode);

	void appendUtf8(const char *s);

	void base64Decode(const char *charset);

	void base64DecodeW(const wchar_t *charset);

	void base64Encode(const char *charset);

	void base64EncodeW(const wchar_t *charset);

	bool beginsWith(const char *s);

	bool beginsWithStr(CkString &s);

	bool beginsWithW(const wchar_t *s);

	char charAt(int idx);

	wchar_t charAtU(int idx);

	void chopAtFirstChar(char c1);

	void chopAtStr(CkString &str);

	void clear(void);

	CkString *clone(void);

	int compareStr(const CkString &str);

	bool containsSubstring(const char *pattern);

	bool containsSubstringNoCase(const char *pattern);

	bool containsSubstringNoCaseW(const wchar_t *substr);

	bool containsSubstringW(const wchar_t *substr);

	int countCharOccurances(char ch);

	void decodeXMLSpecial(void);

	double doubleValue(void);

	void eliminateChar(char ansiChar, int startIndex);

	void encodeXMLSpecial(void);

	bool endsWith(const char *s);

	bool endsWithStr(CkString &s);

	bool endsWithW(const wchar_t *s);

	void entityDecode(void);

	void entityEncode(void);

	bool equals(const char *s);

	bool equalsIgnoreCase(const char *s);

	bool equalsIgnoreCaseStr(CkString &s);

	bool equalsIgnoreCaseW(const wchar_t *s);

	bool equalsStr(CkString &s);

	bool equalsW(const wchar_t *s);

	CkString *getChar(int idx);

	int getNumChars(void);

	int getSizeAnsi(void);

	int getSizeUnicode(void);

	int getSizeUtf8(void);

	const wchar_t *getUnicode(void);

	void hexDecode(const char *charset);

	void hexDecodeW(const wchar_t *charset);

	void hexEncode(const char *charset);

	void hexEncodeW(const wchar_t *charset);

	int indexOf(const char *s);

	int indexOfStr(CkString &s);

	int indexOfW(const wchar_t *s);

	int intValue(void);

	bool isEmpty(void);

	char lastChar(void);

	bool loadFile(const char *fileName, const char *charset);

	bool loadFileW(const wchar_t *path, const wchar_t *charset);

	bool matches(const char *s);

	bool matchesNoCase(const char *s);

	bool matchesNoCaseW(const wchar_t *s);

	bool matchesStr(CkString &str);

	bool matchesW(const wchar_t *s);

	void minimizeMemory(void);

	void obfuscate(void);

	void prepend(const char *s);

	void prependW(const wchar_t *s);

	void qpDecode(const char *charset);

	void qpDecodeW(const wchar_t *charset);

	void qpEncode(const char *charset);

	void qpEncodeW(const wchar_t *charset);

	int removeAll(CkString &str);

	void removeCharOccurances(char c);

	void removeChunk(int charStartPos, int numChars);

	bool removeFirst(CkString &str);

	int replaceAll(CkString &str, CkString &replacement);

	int replaceAllOccurances(const char *pattern, const char *replacement);

	int replaceAllOccurancesW(const wchar_t *pattern, const wchar_t *replacement);

	void replaceChar(char c1, char c2);

	bool replaceFirst(CkString &str, CkString &replacement);

	bool replaceFirstOccurance(const char *pattern, const char *replacement);

	bool replaceFirstOccuranceW(const wchar_t *pattern, const wchar_t *replacement);

	bool saveToFile(const char *filename, const char *charset);

	bool saveToFileW(const wchar_t *path, const wchar_t *charset);

	void setStr(CkString &s);

	void setString(const char *s);

	void setStringAnsi(const char *s);

	void setStringU(const wchar_t *unicode);

	void setStringUtf8(const char *s);

	void shorten(int n);

	CkStringArray *split(char splitChar, bool exceptDoubleQuoted, bool exceptEscaped, bool keepEmpty);

	CkStringArray *split2(const char *splitCharSet, bool exceptDoubleQuoted, bool exceptEscaped, bool keepEmpty);

	CkStringArray *split2W(const wchar_t *splitCharSet, bool exceptDoubleQuoted, bool exceptEscaped, bool keepEmpty);

	CkStringArray *splitAtWS(void);

	CkString *substring(int startCharIdx, int numChars);

	void toCRLF(void);

	void toLF(void);

	void toLowerCase(void);

	void toUpperCase(void);

	CkStringArray *tokenize(const char *punctuation);

	CkStringArray *tokenizeW(const wchar_t *punctuation);

	void trim(void);

	void trim2(void);

	void trimInsideSpaces(void);

	void unobfuscate(void);

	void urlDecode(const char *charset);

	void urlDecodeW(const wchar_t *charset);

	void urlEncode(const char *charset);

	void urlEncodeW(const wchar_t *charset);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
