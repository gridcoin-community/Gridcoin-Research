// CkGzip.h: interface for the CkGzip class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkGzip_H
#define _CkGzip_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;
class CkDateTime;
class CkBaseProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkGzip
class CK_VISIBLE_PUBLIC CkGzip  : public CkMultiByteBase
{
    private:
	CkBaseProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkGzip(const CkGzip &);
	CkGzip &operator=(const CkGzip &);

    public:
	CkGzip(void);
	virtual ~CkGzip(void);

	static CkGzip *createNew(void);
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
	// Specifies an optional comment string that can be embedded within the .gz when
	// any Compress* method is called.
	void get_Comment(CkString &str);
	// Specifies an optional comment string that can be embedded within the .gz when
	// any Compress* method is called.
	const char *comment(void);
	// Specifies an optional comment string that can be embedded within the .gz when
	// any Compress* method is called.
	void put_Comment(const char *newVal);

	// Optional extra-data that can be included within a .gz when a Compress* method is
	// called.
	void get_ExtraData(CkByteData &outBytes);
	// Optional extra-data that can be included within a .gz when a Compress* method is
	// called.
	void put_ExtraData(const CkByteData &inBytes);

	// The filename that is embedded within the .gz during any Compress* method call.
	// When extracting from a .gz using applications such as WinZip, this will be the
	// filename that is created.
	void get_Filename(CkString &str);
	// The filename that is embedded within the .gz during any Compress* method call.
	// When extracting from a .gz using applications such as WinZip, this will be the
	// filename that is created.
	const char *filename(void);
	// The filename that is embedded within the .gz during any Compress* method call.
	// When extracting from a .gz using applications such as WinZip, this will be the
	// filename that is created.
	void put_Filename(const char *newVal);

	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any method call prior to
	// completion. If HeartbeatMs is 0 (the default), no AbortCheck event callbacks
	// will fire.
	int get_HeartbeatMs(void);
	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any method call prior to
	// completion. If HeartbeatMs is 0 (the default), no AbortCheck event callbacks
	// will fire.
	void put_HeartbeatMs(int newVal);

	// The last-modification date/time to be embedded within the .gz when a Compress*
	// method is called. By default, the current system date/time is used.
	void get_LastMod(SYSTEMTIME &outSysTime);
	// The last-modification date/time to be embedded within the .gz when a Compress*
	// method is called. By default, the current system date/time is used.
	void put_LastMod(const SYSTEMTIME &sysTime);

	// The same as the LastMod property, but allows the date/time to be get/set in
	// RFC822 string format.
	void get_LastModStr(CkString &str);
	// The same as the LastMod property, but allows the date/time to be get/set in
	// RFC822 string format.
	const char *lastModStr(void);
	// The same as the LastMod property, but allows the date/time to be get/set in
	// RFC822 string format.
	void put_LastModStr(const char *newVal);

	// If set to true, the file produced by an Uncompress* method will use the
	// current date/time for the last-modification date instead of the date/time found
	// within the Gzip format.
	bool get_UseCurrentDate(void);
	// If set to true, the file produced by an Uncompress* method will use the
	// current date/time for the last-modification date instead of the date/time found
	// within the Gzip format.
	void put_UseCurrentDate(bool newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Compresses a file to create a GZip compressed file (.gz).
	bool CompressFile(const char *srcPath, const char *destPath);

	// Compresses a file to create a GZip compressed file (.gz). inFilename is the actual
	// filename on disk.  embeddedFilename is the filename to be embedded in the .gz such that when
	// it is un-gzipped, this is the name of the file that will be created.
	bool CompressFile2(const char *srcPath, const char *embeddedFilename, const char *destPath);

	// Gzip compresses a file to an in-memory image of a .gz file.
	bool CompressFileToMem(const char *inFilename, CkByteData &outData);

	// Gzip compresses and creates a .gz file from in-memory data.
	bool CompressMemToFile(const CkByteData &inData, const char *destPath);

	// Compresses in-memory data to an in-memory image of a .gz file.
	bool CompressMemory(const CkByteData &inData, CkByteData &outData);

	// Gzip compresses a string and writes the output to a byte array. The string is
	// first converted to the charset specified by ARG2. Typical charsets are "utf-8",
	// "iso-8859-1", "shift_JIS", etc.
	bool CompressString(const char *inStr, const char *destCharset, CkByteData &outBytes);

	// The same as CompressString, except the binary output is returned in encoded
	// string form according to the  encoding. The  encoding can be any of the following:
	// "Base64", "modBase64", "Base32", "UU", "QP" (for quoted-printable), "URL" (for
	// url-encoding), "Hex", "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", and
	// "url_rfc3986".
	bool CompressStringENC(const char *strIn, const char *charset, const char *encoding, CkString &outStr);
	// The same as CompressString, except the binary output is returned in encoded
	// string form according to the  encoding. The  encoding can be any of the following:
	// "Base64", "modBase64", "Base32", "UU", "QP" (for quoted-printable), "URL" (for
	// url-encoding), "Hex", "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", and
	// "url_rfc3986".
	const char *compressStringENC(const char *strIn, const char *charset, const char *encoding);

	// Gzip compresses a string and writes the output to a .gz compressed file. The
	// string is first converted to the charset specified by ARG2. Typical charsets are
	// "utf-8", "iso-8859-1", "shift_JIS", etc.
	bool CompressStringToFile(const char *inStr, const char *destCharset, const char *destPath);

	// Decodes an encoded string and returns the original data. The encoding mode is
	// determined by  encoding. It may be "base64", "hex", "quoted-printable", or "url".
	bool Decode(const char *str, const char *encoding, CkByteData &outBytes);

	// Provides the ability to use the zip/gzip's deflate algorithm directly to
	// compress a string. Internal to this method, inString is first converted to the
	// charset specified by  charsetName. The data is then compressed using the deflate
	// compression algorithm. The binary output is then encoded according to  outputEncoding.
	// Possible values for  outputEncoding are "hex", "base64", "url", and "quoted-printable".
	// 
	// Note: The output of this method is compressed data with no Gzip file format. Use
	// the Compress* methods to produce Gzip file format output.
	// 
	bool DeflateStringENC(const char *strIn, const char *charset, const char *encoding, CkString &outStr);
	// Provides the ability to use the zip/gzip's deflate algorithm directly to
	// compress a string. Internal to this method, inString is first converted to the
	// charset specified by  charsetName. The data is then compressed using the deflate
	// compression algorithm. The binary output is then encoded according to  outputEncoding.
	// Possible values for  outputEncoding are "hex", "base64", "url", and "quoted-printable".
	// 
	// Note: The output of this method is compressed data with no Gzip file format. Use
	// the Compress* methods to produce Gzip file format output.
	// 
	const char *deflateStringENC(const char *strIn, const char *charset, const char *encoding);

	// Encodes binary data to a printable string. The encoding mode is determined by
	//  encoding. It may be "base64", "hex", "quoted-printable", or "url".
	bool Encode(const CkByteData &byteData, const char *encoding, CkString &outStr);
	// Encodes binary data to a printable string. The encoding mode is determined by
	//  encoding. It may be "base64", "hex", "quoted-printable", or "url".
	const char *encode(const CkByteData &byteData, const char *encoding);

	// Determines if the inGzFilename is a Gzip formatted file. Returns true if it is a Gzip
	// formatted file, otherwise returns false.
	bool ExamineFile(const char *inGzPath);

	// Determines if the in-memory bytes (inGzData) contain a Gzip formatted file. Returns
	// true if it is Gzip format, otherwise returns false.
	bool ExamineMemory(const CkByteData &inGzData);

	// Gets the last-modification date/time to be embedded within the .gz.
	// The caller is responsible for deleting the object returned by this method.
	CkDateTime *GetDt(void);

	// This the reverse of DeflateStringENC.
	// 
	// The input string is first decoded according to  inputEncoding. (Possible values for  inputEncoding
	// are "hex", "base64", "url", and "quoted-printable".) The compressed data is then
	// inflated, and the result is then converted from  convertFromCharset (if necessary) to return a
	// string.
	// 
	bool InflateStringENC(const char *strIn, const char *charset, const char *encoding, CkString &outStr);
	// This the reverse of DeflateStringENC.
	// 
	// The input string is first decoded according to  inputEncoding. (Possible values for  inputEncoding
	// are "hex", "base64", "url", and "quoted-printable".) The compressed data is then
	// inflated, and the result is then converted from  convertFromCharset (if necessary) to return a
	// string.
	// 
	const char *inflateStringENC(const char *strIn, const char *charset, const char *encoding);

	// Returns true if the component has been unlocked.
	bool IsUnlocked(void);

	// Reads a binary file into memory and returns the byte array. Note: This method
	// does not parse a Gzip, it is only a convenience method for reading a binary file
	// into memory.
	bool ReadFile(const char *path, CkByteData &outBytes);

	// Sets the last-modification date/time to be embedded within the .gz when a
	// Compress* method is called. If not explicitly set, the current system date/time
	// is used.
	bool SetDt(CkDateTime &dt);

	// Unpacks a .tar.gz file. The ungzip and untar occur in streaming mode. There are
	// no temporary files and the memory footprint is constant (and small) while
	// untarring.  bNoAbsolute may be true or false. A value of true protects from
	// untarring to absolute paths. (For example, imagine the trouble if the tar
	// contains files with absolute paths beginning with /Windows/system32)
	bool UnTarGz(const char *gzFilename, const char *destDir, bool bNoAbsolute);

	// Un-Gzips a .gz file. The output filename is specified by the 2nd argument and
	// not by the filename embedded within the .gz.
	bool UncompressFile(const char *srcPath, const char *destPath);

	// Un-Gzips a .gz file directly to memory.
	bool UncompressFileToMem(const char *inFilename, CkByteData &outData);

	// Uncompresses a .gz file that contains a text file. The contents of the text file
	// are returned as a string. The character encoding of the text file is specified
	// by  charset. Typical charsets are "iso-8859-1", "utf-8", "windows-1252",
	// "shift_JIS", "big5", etc.
	bool UncompressFileToString(const char *inFilename, const char *inCharset, CkString &outStr);
	// Uncompresses a .gz file that contains a text file. The contents of the text file
	// are returned as a string. The character encoding of the text file is specified
	// by  charset. Typical charsets are "iso-8859-1", "utf-8", "windows-1252",
	// "shift_JIS", "big5", etc.
	const char *uncompressFileToString(const char *inFilename, const char *inCharset);

	// Un-Gzips from an in-memory image of a .gz file to a file.
	bool UncompressMemToFile(const CkByteData &inData, const char *destPath);

	// Un-Gzips from an in-memory image of a .gz file directly into memory.
	bool UncompressMemory(const CkByteData &inData, CkByteData &outData);

	// The reverse of CompressString.
	// 
	// The bytes in inData are uncompressed, then converted from  inCharset (if necessary) to
	// return a string.
	// 
	bool UncompressString(const CkByteData &inData, const char *inCharset, CkString &outStr);
	// The reverse of CompressString.
	// 
	// The bytes in inData are uncompressed, then converted from  inCharset (if necessary) to
	// return a string.
	// 
	const char *uncompressString(const CkByteData &inData, const char *inCharset);

	// The same as UncompressString, except the compressed data is provided in encoded
	// string form based on the value of  encoding. The  encoding can be "Base64", "modBase64",
	// "Base32", "UU", "QP" (for quoted-printable), "URL" (for url-encoding), "Hex",
	// "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", and "url_rfc3986".
	bool UncompressStringENC(const char *strIn, const char *charset, const char *encoding, CkString &outStr);
	// The same as UncompressString, except the compressed data is provided in encoded
	// string form based on the value of  encoding. The  encoding can be "Base64", "modBase64",
	// "Base32", "UU", "QP" (for quoted-printable), "URL" (for url-encoding), "Hex",
	// "Q", "B", "url_oath", "url_rfc1738", "url_rfc2396", and "url_rfc3986".
	const char *uncompressStringENC(const char *strIn, const char *charset, const char *encoding);

	// Unlocks the component allowing for the full functionality to be used.
	bool UnlockComponent(const char *unlockCode);

	// A convenience method for writing a binary byte array to a file.
	bool WriteFile(const char *path, const CkByteData &binaryData);

	// Converts base64-gzip .xfdl data to a decompressed XML string. The xfldData contains
	// the base64 data. This method returns the decoded/decompressed XML string.
	bool XfdlToXml(const char *xfdl, CkString &outStr);
	// Converts base64-gzip .xfdl data to a decompressed XML string. The xfldData contains
	// the base64 data. This method returns the decoded/decompressed XML string.
	const char *xfdlToXml(const char *xfdl);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
