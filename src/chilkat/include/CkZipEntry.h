// CkZipEntry.h: interface for the CkZipEntry class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkZipEntry_H
#define _CkZipEntry_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;
class CkDateTime;
class CkBaseProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkZipEntry
class CK_VISIBLE_PUBLIC CkZipEntry  : public CkMultiByteBase
{
    private:
	CkBaseProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkZipEntry(const CkZipEntry &);
	CkZipEntry &operator=(const CkZipEntry &);

    public:
	CkZipEntry(void);
	virtual ~CkZipEntry(void);

	static CkZipEntry *createNew(void);
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
	// The comment stored within the Zip for this entry.
	void get_Comment(CkString &str);
	// The comment stored within the Zip for this entry.
	const char *comment(void);
	// The comment stored within the Zip for this entry.
	void put_Comment(const char *newVal);

	// The size in bytes of this entry's file data when compression is applied
	unsigned long get_CompressedLength(void);

	// The size in bytes of this entry's file data when compression is applied
	__int64 get_CompressedLength64(void);

	// The size in decimal string format of this file when Zip compression is applied.
	void get_CompressedLengthStr(CkString &str);
	// The size in decimal string format of this file when Zip compression is applied.
	const char *compressedLengthStr(void);

	// The compression level. 0 = no compression, while 9 = maximum compression. The
	// default is 6.
	int get_CompressionLevel(void);
	// The compression level. 0 = no compression, while 9 = maximum compression. The
	// default is 6.
	void put_CompressionLevel(int newVal);

	// Set to 0 for no compression, or 8 for the Deflate algorithm. The Deflate
	// algorithm is the default algorithm of the most popular Zip utility programs,
	// such as WinZip
	int get_CompressionMethod(void);
	// Set to 0 for no compression, or 8 for the Deflate algorithm. The Deflate
	// algorithm is the default algorithm of the most popular Zip utility programs,
	// such as WinZip
	void put_CompressionMethod(int newVal);

	// The CRC for the zip entry. For AES encrypted entries, the CRC value will be 0.
	// (See http://www.winzip.com/aes_info.htm#CRC )
	int get_Crc(void);

	// The unique ID assigned by Zip.NET while the object is instantiated in memory.
	int get_EntryID(void);

	// Indicates the origin of the entry. There are three possible values: (0) an entry
	// in an existing Zip file, (1) a file on disk referenced by Zip.NET and to be
	// added to the Zip, and (2) an entry created directly from data in memory.
	int get_EntryType(void);

	// The local last-modified date/time.
	void get_FileDateTime(SYSTEMTIME &outSysTime);
	// The local last-modified date/time.
	void put_FileDateTime(const SYSTEMTIME &sysTime);

	// The local last-modified date/time in RFC822 string format.
	void get_FileDateTimeStr(CkString &str);
	// The local last-modified date/time in RFC822 string format.
	const char *fileDateTimeStr(void);
	// The local last-modified date/time in RFC822 string format.
	void put_FileDateTimeStr(const char *newVal);

	// The file name of the Zip entry.
	void get_FileName(CkString &str);
	// The file name of the Zip entry.
	const char *fileName(void);
	// The file name of the Zip entry.
	void put_FileName(const char *newVal);

	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort inflate/extract/unzip calls
	// prior to completion. If HeartbeatMs is 0 (the default), no AbortCheck event
	// callbacks will fire.
	int get_HeartbeatMs(void);
	// The number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort inflate/extract/unzip calls
	// prior to completion. If HeartbeatMs is 0 (the default), no AbortCheck event
	// callbacks will fire.
	void put_HeartbeatMs(int newVal);

	// True if the Zip entry is a directory, false if it is a file.
	bool get_IsDirectory(void);

	// Controls whether the "text flag" of the internal file attributes for this entry
	// within the zip is set. This is a bit flag that indicates whether the file
	// contents are text or binary. It is for informational use and it is not
	// imperative that this bit flag is accurately set. The ability to set this bit
	// flag is only provided to satisfy certain cases where another application might
	// be sensitive to the flag.
	bool get_TextFlag(void);
	// Controls whether the "text flag" of the internal file attributes for this entry
	// within the zip is set. This is a bit flag that indicates whether the file
	// contents are text or binary. It is for informational use and it is not
	// imperative that this bit flag is accurately set. The ability to set this bit
	// flag is only provided to satisfy certain cases where another application might
	// be sensitive to the flag.
	void put_TextFlag(bool newVal);

	// The size in bytes of this entry's file data when uncompressed.
	unsigned long get_UncompressedLength(void);

	// The size in bytes of this entry's file data when uncompressed.
	__int64 get_UncompressedLength64(void);

	// The size in bytes (in decimal string format) of this zip entry's data when
	// uncompressed.
	void get_UncompressedLengthStr(CkString &str);
	// The size in bytes (in decimal string format) of this zip entry's data when
	// uncompressed.
	const char *uncompressedLengthStr(void);

	// A string containing the hex encoded raw filename bytes found in the Zip entry.
	void get_FileNameHex(CkString &str);
	// A string containing the hex encoded raw filename bytes found in the Zip entry.
	const char *fileNameHex(void);



	// ----------------------
	// Methods
	// ----------------------
	// Appends binary data to the zip entry's file contents.
	bool AppendData(const CkByteData &inData);

	// Appends text data to the zip entry's file contents. The text is appended using
	// the character encoding specified by the  charset, which can be "utf-8", "ansi", etc.
	bool AppendString(const char *inStr, const char *charset);

	// Returns the compressed data as a byte array.
	bool Copy(CkByteData &outData);

	// Returns the compressed data as a Base64-encoded string. This data can be
	// re-added to a Zip by calling Zip.AppendBase64.
	bool CopyToBase64(CkString &outStr);
	// Returns the compressed data as a Base64-encoded string. This data can be
	// re-added to a Zip by calling Zip.AppendBase64.
	const char *copyToBase64(void);

	// Returns the compressed data as a hexidecimal encoded string. This data can be
	// re-added to a Zip by calling Zip.AppendHex.
	bool CopyToHex(CkString &outStr);
	// Returns the compressed data as a hexidecimal encoded string. This data can be
	// re-added to a Zip by calling Zip.AppendHex.
	const char *copyToHex(void);

	// Unzips this entry into the specified base directory. The file is unzipped to the
	// subdirectory according to the relative path stored with the entry in the Zip.
	// Use ExtractInto to unzip into a specific directory regardless of the path
	// information stored in the Zip.
	bool Extract(const char *dirPath);

	// Unzip a file into a specific directory
	bool ExtractInto(const char *dirPath);

	// Returns the last-modified date/time of this zip entry.
	// The caller is responsible for deleting the object returned by this method.
	CkDateTime *GetDt(void);

	// Inflate a file within a Zip directly into memory.
	bool Inflate(CkByteData &outData);

	// Return the next entry (file or directory) within the Zip
	// The caller is responsible for deleting the object returned by this method.
	CkZipEntry *NextEntry(void);

	// Replaces the zip entry's existing contents with new data.
	bool ReplaceData(const CkByteData &inData);

	// Replaces the zip entry's existing contents with new text data. The text will be
	// stored using the character encoding as specified by  charset, which can be "utf-8",
	// "ansi", etc.
	bool ReplaceString(const char *inStr, const char *charset);

	// Sets the last-modified date/time for this zip entry.
	void SetDt(CkDateTime &dt);

	// Inflate and return the uncompressed data as a string The lineEndingBehavior is as follows:
	// 
	// 0 = leave unchanged.
	// 1 = convert all to bare LF's
	// 2 = convert all to CRLF's
	// 
	// The  srcCharset tells the component how to interpret the bytes of the uncompressed file
	// -- i.e. as utf-8, utf-16, windows-1252, etc.
	bool UnzipToString(int lineEndingBehavior, const char *srcCharset, CkString &outStr);
	// Inflate and return the uncompressed data as a string The lineEndingBehavior is as follows:
	// 
	// 0 = leave unchanged.
	// 1 = convert all to bare LF's
	// 2 = convert all to CRLF's
	// 
	// The  srcCharset tells the component how to interpret the bytes of the uncompressed file
	// -- i.e. as utf-8, utf-16, windows-1252, etc.
	const char *unzipToString(int lineEndingBehavior, const char *srcCharset);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
