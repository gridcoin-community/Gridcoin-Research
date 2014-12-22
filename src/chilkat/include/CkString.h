// CkString.h: interface for the CkString class.
//
//////////////////////////////////////////////////////////////////////


#ifndef _CKSTRING_H
#define _CKSTRING_H

#include "chilkatDefs.h"

#include "CkObject.h"


class CkStringArray;

#ifndef __sun__
#pragma pack (push, 8)
#endif
 


// CLASS: CkString
class CkString : public CkObject
{
    public:
	CkString();
	~CkString();

	static CkString *createNew(void);

	CkString(const CkString &);
	CkString &operator=(const CkString &);

	CkString &operator=(const char *);
	CkString &operator=(int);
	CkString &operator=(bool);
	CkString &operator=(char);

       // 
       // operator to cast to a const char *
       // 
       operator const char *();
       operator const wchar_t *();


        void appendHexData(const char *pByteData, unsigned long szByteData);

	// BEGIN PUBLIC INTERFACE
	bool get_Utf8(void) const;
	void put_Utf8(bool b);


	int get_NumArabic(void);
	int get_NumAscii(void);
	int get_NumCentralEuro(void);  // such as Czech, Polish, etc.
	int get_NumChinese(void);
	int get_NumCyrillic(void);
	int get_NumGreek(void);
	int get_NumHebrew(void);
	int get_NumJapanese(void);
	int get_NumKorean(void);
	int get_NumLatin(void);	    // Western European
	int get_NumThai(void);


	// Load the contents of a text file into the CkString object.
	// The string is cleared before loading.
	// Charset can be "unicode", "ansi", "utf-8", or any other charset
	// such as "iso-8859-1", "windows-1253", "Shift_JIS", etc.
        bool loadFile(const char *path, const char *charset);
	bool loadFileW(const wchar_t *path, const wchar_t *charset);

	// Returns the ANSI character starting at an index.
	// The first character is at index 0.
	// IMPORTANT: This is not a byte index, but a character position index.
	char charAt(int idx) const;
	wchar_t charAtU(int idx) const;   // Returns Unicode character at position idx.

	// If this string is "43" for example, this returns 43.
        int intValue(void) const;
        double doubleValue(void) const;

	CkString *clone(void) const;
	void setStr(CkString &s);

	bool endsWith(const char *s) const;
	bool endsWithW(const wchar_t *s) const;
	bool endsWithStr(CkString &s) const;

	bool beginsWithStr(CkString &s) const;
	int indexOf(const char *s) const;
	int indexOfW(const wchar_t *s) const;

	int indexOfStr(CkString &s) const;
        int replaceAll(CkString &str, CkString &replacement);
        bool replaceFirst(CkString &str, CkString &replacement);
	CkString *substring(int startCharIdx, int numChars) const;
	bool matchesStr(CkString &str) const;

	bool matches(const char *s) const;
	bool matchesNoCase(const char *s) const;
	bool matchesW(const wchar_t *s) const;
	bool matchesNoCaseW(const wchar_t *s) const;

	CkString *getChar(int idx) const;
        int removeAll(CkString &str);
        bool removeFirst(CkString &str);
	void chopAtStr(CkString &str);

	void urlDecode(const char *charset);
	void urlEncode(const char *charset);
	void base64Decode(const char *charset);
	void base64Encode(const char *charset);
	void qpDecode(const char *charset);
	void qpEncode(const char *charset);
	void hexDecode(const char *charset);
	void hexEncode(const char *charset);

	void urlDecodeW(const wchar_t *charset);
	void urlEncodeW(const wchar_t *charset);
	void base64DecodeW(const wchar_t *charset);
	void base64EncodeW(const wchar_t *charset);
	void qpDecodeW(const wchar_t *charset);
	void qpEncodeW(const wchar_t *charset);
	void hexDecodeW(const wchar_t *charset);
	void hexEncodeW(const wchar_t *charset);

	void entityDecode(void);
	void entityEncode(void);
	void appendUtf8(const char *s);	    
	void appendAnsi(const char *s);	   
	
	bool isEmpty(void);

	// Appends the current local date/time.
	void appendCurrentDateRfc822(void);

	// SYSTEMTIME is defined in <windows.h>
	// For more information, Google "SYSTEMTIME"
	// sysTime should represent a local date/time
	void appendDateRfc822(SYSTEMTIME &sysTime);

	void appendDateRfc822Gmt(SYSTEMTIME &sysTime);

	// Self explanatory, right?
	void clear(void);
	void prepend(const char *s);	    
	void prependW(const wchar_t *s);

	void appendInt(int n);
	void append(const char *s);	    
        void appendChar(char c);
        void appendCharU(wchar_t c);
	void appendN(const char *pByteData, unsigned long szByteData);	
        void appendStr(const CkString &str);
	void appendEnc(const char *s, const char *charEncoding);	// such as "utf-8", "windows-1252", "shift_JIS", etc.
	void appendRandom(int numBytes, const char *encoding);	// such as "base64", "hex", "qp", "url", etc.

	// Convert the binary data to a hex string representation and append.
        void appendHexData(const unsigned char *pByteData, unsigned long szByteData);

	// Same as clearing the string and appending.
        void setString(const char *s);
        void setStringAnsi(const char *s);
        void setStringUtf8(const char *s);

	// To/From Unicode (wchar_t)
	// On some systems wchar_t is utf-16, on others it is utf-32.
	void setStringU(const wchar_t *unicode);
	void appendU(const wchar_t *unicode);
	void appendNU(const wchar_t *unicode, int numChars);
	const wchar_t *getUnicode(void) const;

	// Returns utf16 chars having the byte order of the local computer.
	// (Returns utf16LE on little-endian machines, and utf16BE on big-endian machines)
	const uint16_t *getUtf16(void) const;

	// To ANSI...
	const char *getAnsi(void) const;
	const char *getStringAnsi(void) const { return getAnsi(); }

	// To utf-8...
	const char *getUtf8(void) const;
	const char *getStringUtf8(void) const { return getUtf8(); }

	// Get any multi-byte encoding.
	const char *getEnc(const char *encoding);
	const char *getEncW(const wchar_t *encoding);

	// Same as strcmp
	int compareStr(const CkString &str) const;	// Compare against another CkString

	const char *getString(void) const;

	int getSizeUtf8(void) const;  // Returns size in bytes of utf-8 string.
	int getSizeAnsi(void) const; // Returns size in bytes of ANSI string.
	int getSizeUnicode(void) const; // Returns size in bytes of Unicode string.
	int getNumChars(void) const;	// Returns number of characters in the string.

        // Trim whitespace from both ends of the string.
        void trim(void);    // Does not include newline
        void trim2(void);   // Includes newline
	void trimInsideSpaces(void);

        // Case sensitive replacement functions.
	// Return the number of occurances replaced.
        int replaceAllOccurances(const char *pattern, const char *replacement);
	int replaceAllOccurancesW(const wchar_t *pattern, const wchar_t *replacement);

        bool replaceFirstOccurance(const char *pattern, const char *replacement);
	bool replaceFirstOccuranceW(const wchar_t *pattern, const wchar_t *replacement);

	// Searches the string for the 1st occurance of startStr.  Then 
	// searches for the first occurance of endStr after startStr.
	// If both startStr and endStr are found, then replaces all occurances
	// of  pattern with replacement.
	// Then continues past the endStr searching for the next occurance of StartStr
	// and repeating until replacements are made within all chunks delimited by
	// startStr/endStr are made.

	// For example, if the content is XML and has nodes such as "<description>.....</description>",
	// then the startStr and endStr can be "<description>" and "</description>" respectively,
	// and the next within each pair can be search/replaced.

	// Return the number of occurances replaced.
	int replaceAllOccurancesBetween(const char *startStr, 
	    const char *endStr,
	    const char *pattern, 
	    const char *replacement);

	int replaceAllOccurancesBetweenW(const wchar_t *startStr, 
	    const wchar_t *endStr,
	    const wchar_t *pattern, 
	    const wchar_t *replacement);


        // CRLF
        void toCRLF(void);                  // Make all end of lines a CRLF ("\r\n")
        void toLF(void);                    // Make all end of lines a "\n"

        // Eliminate all occurances of a particular ANSI character.
        void eliminateChar(char ansiChar, int startIndex);

        // Return the last (ANSI) character in the string.
        char lastChar(void);

	// Return the number of occurances of ch in the string.
	int countCharOccurances(char ch);

        // Remove the last N chars from the string.
        void shorten(int n);    

        // Convert to lower or upper case
        void toLowerCase(void);
        void toUpperCase(void);

        // Convert XML special characters to their representations
        // Example: '<' is converted to &lt;
        void encodeXMLSpecial(void);    // Convert '<' to &lt; (etc.)
        void decodeXMLSpecial(void);    // Convert &lt; to '<' (etc.)

        bool containsSubstring(const char *substr) const;	
        bool containsSubstringNoCase(const char *substr) const;	
        bool containsSubstringW(const wchar_t *substr) const;	
        bool containsSubstringNoCaseW(const wchar_t *substr) const;	

	// For many Win32 SDK functions, such as CreateFile, error information
	// must be retrieved by using the Win32 functions GetLastError and FormatMessage.
	// This method calls these Win32 functions to format the error and append it
	// to the string.
#ifdef WIN32
        void appendLastWindowsError(void);
#endif
	// Returns true if the strings are equal, or false.  (Not the same as 
	// "compare", which returns 0 if equal, and 1 or -1 if the strings are lexicographically
	// less than or greater than)
        bool equals(const char *s) const;   
        bool equalsIgnoreCase(const char *s) const;

        bool equalsStr(CkString &s) const;
        bool equalsIgnoreCaseStr(CkString &s) const;

        bool equalsW(const wchar_t *s) const;   
        bool equalsIgnoreCaseW(const wchar_t *s) const;

        // Remove a chunk from the string.
        void removeChunk(int charStartPos, int numChars);

        // Remove all occurances of a particular character.
        void removeCharOccurances(char c);
        void removeCharOccurancesU(wchar_t c);

	// Replace all occurance of c1 with c2.
	void replaceChar(char c1, char c2);
	void replaceCharU(wchar_t c1, wchar_t c2);

	// Replace the first occurance of c1 with a null terminator
	void chopAtFirstChar(char c1);

	void obfuscate(void);
	void unobfuscate(void);

	// Save the string to a file.
	// charset can be "ansi", "utf-8", "unicode", or anything else such as "iso-8859-1"
	bool saveToFile(const char *path, const char *charset);
	bool saveToFileW(const wchar_t *path, const wchar_t *charset);

	CkStringArray *split(char splitChar, bool exceptDoubleQuoted, bool exceptEscaped, bool keepEmpty) const;
	CkStringArray *split2(const char *splitCharSet, bool exceptDoubleQuoted, bool exceptEscaped, bool keepEmpty) const;
	CkStringArray *split2W(const wchar_t *splitCharSet, bool exceptDoubleQuoted, bool exceptEscaped, bool keepEmpty) const;
	CkStringArray *tokenize(const char *punctuation) const;
	CkStringArray *tokenizeW(const wchar_t *punctuation) const;

	// Equivalent to split2(" \t\r\n",true,true,false)
	CkStringArray *splitAtWS(void) const;

	// Return true if this string begins with substr (case sensitive)
	bool beginsWith(const char *sSubstr) const;
	bool beginsWithW(const wchar_t *s) const;

	CkString *getDelimited(const char *beginSearchAfter, 
	       const char *startDelim, const char *endDelim);
	CkString *getDelimitedW(const wchar_t *beginSearchAfter, 
	       const wchar_t *startDelim, const wchar_t *endDelim);

	void minimizeMemory(void);

	// STRING_INSERT_POINT

	// END PUBLIC INTERFACE

	bool isInternalPtr(const char *str);

    private:
	// Internal implementation.
	void *m_x;
	bool m_utf8;	// If true, all input "const char *" arguments are utf-8, otherwise they are ANSI strings.
	void *m_sb;	// Used for getEnc()

    public:
	// For internal use
	void *getImplX(void) const { return m_x; }

};
#ifndef __sun__
#pragma pack (pop)
#endif


#endif
