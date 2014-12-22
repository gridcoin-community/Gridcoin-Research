// CkFileAccess.h: interface for the CkFileAccess class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkFileAccess_H
#define _CkFileAccess_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;
class CkDateTime;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkFileAccess
class CK_VISIBLE_PUBLIC CkFileAccess  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkFileAccess(const CkFileAccess &);
	CkFileAccess &operator=(const CkFileAccess &);

    public:
	CkFileAccess(void);
	virtual ~CkFileAccess(void);

	static CkFileAccess *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// Returns true if the current open file is at the end-of-file.
	bool get_EndOfFile(void);

	// This property is set by the following methods: FileOpen, OpenForRead,
	// OpenForWrite, OpenForReadWrite, and OpenForAppend. It provides an error code
	// indicating the failure reason. Possible values are:
	// 
	//     Success (No error)
	//     Access denied.
	//     File not found.
	//     General (non-specific) open error.
	//     File aleady exists.
	//     Path refers to a directory and the access requested involves writing.
	//     Too many symbolic links were encountered in resolving path.
	//     The process already has the maximum number of files open.
	//     Pathname is too long.
	//     The system limit on the total number of open files has been reached.
	//     Pathname refers to a device special file and no corresponding device exists.
	//     Insufficient kernel memory was available.
	//     Pathname was to be created but the device containing pathname has no room
	//     for the new file.
	//     A component used as a directory in pathname is not, in fact, a directory.
	//     Pathname refers to a regular file, too large to be opened (this would be a
	//     limitation of the underlying operating system, not a limitation imposed by
	//     Chilkat).
	//     Pathname refers to a file on a read-only filesystem and write access was
	//     requested.
	// 
	int get_FileOpenError(void);

	// The error message text associated with the FileOpenError code.
	void get_FileOpenErrorMsg(CkString &str);
	// The error message text associated with the FileOpenError code.
	const char *fileOpenErrorMsg(void);

	// The current working directory of the calling process.
	void get_CurrentDir(CkString &str);
	// The current working directory of the calling process.
	const char *currentDir(void);



	// ----------------------
	// Methods
	// ----------------------
	// Appends a string using the ANSI character encoding to the currently open file.
	bool AppendAnsi(const char *text);

	// Appends a string using the character encoding specified by str to the currently
	// open file.
	bool AppendText(const char *text, const char *charset);

	// Appends the 2-byte Unicode BOM (little endian) to the currently open file.
	bool AppendUnicodeBOM(void);

	// Appends the 3-byte utf-8 BOM to the currently open file.
	bool AppendUtf8BOM(void);

	// Creates all directories necessary such that the entire dirPath exists.
	bool DirAutoCreate(const char *path);

	// Creates a new directory specified by dirPath.
	bool DirCreate(const char *path);

	// Deletes the directory specified by dirPath.
	bool DirDelete(const char *path);

	// Same as DirAutoCreate, except the argument is a file path (the last part of the
	// path is a filename and not a directory). Creates all missing directories such
	// that filePath may be created.
	bool DirEnsureExists(const char *filePath);

	// Closes the currently open file.
	void FileClose(void);

	// Compares the contents of two files and returns true if they are equal and
	// otherwise returns false. The actual contents of the files are only compared if
	// the sizes are equal. The files are not entirely loaded into memory. Instead,
	// they are compared chunk by chunk. This allows for any size files to be compared,
	// regardless of the memory capacity of the computer.
	bool FileContentsEqual(const char *path1, const char *path2);

	// Copys existingFilepath to  newFilepath. If  failIfExists is true and  newFilepath already exists, then an error is
	// returned.
	bool FileCopy(const char *existing, const char *newFilename, bool failIfExists);

	// Deletes the file specified by filePath.
	bool FileDelete(const char *filename);

	// Returns true if filePath exists, otherwise returns false.
	bool FileExists(const char *filename);

	// This method should only be called on Windows operating systems. It's arguments
	// are similar to the Windows Platform SDK function named CreateFile. For Linux,
	// MAC OS X, and other operating system, use the OpenForRead, OpenForWrite,
	// OpenForReadWrite, and OpenForAppend methods.
	// 
	// Opens a file for reading or writing. The arguments mirror the Windows CreateFile
	// function:
	// Access Modes:
	// GENERIC_READ	(0x80000000)
	// GENERIC_WRITE (0x40000000)
	// 
	// Share Modes:
	// FILE_SHARE_READ(0x00000001)
	// FILE_SHARE_WRITE(0x00000002)
	// 
	// Create Dispositions
	// CREATE_NEW          1
	// CREATE_ALWAYS       2
	// OPEN_EXISTING       3
	// OPEN_ALWAYS         4
	// TRUNCATE_EXISTING   5
	// 
	// // Attributes:
	// FILE_ATTRIBUTE_READONLY         0x00000001
	// FILE_ATTRIBUTE_HIDDEN           0x00000002
	// FILE_ATTRIBUTE_SYSTEM           0x00000004
	// FILE_ATTRIBUTE_DIRECTORY        0x00000010
	// FILE_ATTRIBUTE_ARCHIVE          0x00000020
	// FILE_ATTRIBUTE_NORMAL           0x00000080
	// FILE_ATTRIBUTE_TEMPORARY	   0x00000100
	// 
	bool FileOpen(const char *filename, unsigned long accessMode, unsigned long shareMode, unsigned long createDisp, unsigned long attr);

	// Reads bytes from the currently open file. maxNumBytes specifies the maximum number of
	// bytes to read. Returns an empty byte array on error.
	bool FileRead(int numBytes, CkByteData &outBytes);

	// Renames a file from existingFilepath to  newFilepath.
	bool FileRename(const char *existing, const char *newFilename);

	// Sets the file pointer for the currently open file. The offset is an offset in
	// bytes from the  origin. The  origin can be one of the following:
	// 0 = Offset is from beginning of file.
	// 1 = Offset is from current position of file pointer.
	// 2 = Offset is from the end-of-file (offset may be negative).
	bool FileSeek(int offset, int origin);

	// Returns the size, in bytes, of a file. Returns -1 for failure.
	int FileSize(const char *filename);

	// Writes bytes to the currently open file.
	bool FileWrite(const CkByteData &data);

	// Creates a temporary filepath of the form dirPath\ prefix_xxxx.TMP Where "xxxx" are
	// random alpha-numeric chars. The returned filepath is guaranteed to not already
	// exist.
	bool GetTempFilename(const char *dirName, const char *prefix, CkString &outStr);
	// Creates a temporary filepath of the form dirPath\ prefix_xxxx.TMP Where "xxxx" are
	// random alpha-numeric chars. The returned filepath is guaranteed to not already
	// exist.
	const char *getTempFilename(const char *dirName, const char *prefix);
	// Creates a temporary filepath of the form dirPath\ prefix_xxxx.TMP Where "xxxx" are
	// random alpha-numeric chars. The returned filepath is guaranteed to not already
	// exist.
	const char *tempFilename(const char *dirName, const char *prefix);

	// Opens a file for appending. If filePath did not already exists, it is created. When
	// an existing file is opened with this method, the contents will not be
	// overwritten and the file pointer is positioned at the end of the file.
	// 
	// If the open/create failed, then error information will be available in the
	// FileOpenError and FileOpenErrorMsg properties.
	// 
	bool OpenForAppend(const char *filePath);

	// Opens a file for reading. The file may contain any type of data (binary or text)
	// and it must already exist. If the open failed, then error information will be
	// available in the FileOpenError and FileOpenErrorMsg properties.
	bool OpenForRead(const char *filePath);

	// Opens a file for reading/writing. If filePath did not already exists, it is created.
	// When an existing file is opened with this method, the contents will not be
	// overwritten, but the file pointer is positioned at the beginning of the file.
	// 
	// If the open/create failed, then error information will be available in the
	// FileOpenError and FileOpenErrorMsg properties.
	// 
	bool OpenForReadWrite(const char *filePath);

	// Opens a file for writing. If filePath did not already exists, it is created. When an
	// existing file is opened with this method, the contents will be overwritten. (For
	// example, calling OpenForWrite on an existing file and then immediately closing
	// the file will result in an empty file.) If the open/create failed, then error
	// information will be available in the FileOpenError and FileOpenErrorMsg
	// properties.
	bool OpenForWrite(const char *filePath);

	// Reads the entire contents of a binary file and returns it as an encoded string
	// (using an encoding such as Base64, Hex, etc.) The  encoding may be one of the
	// following strings: base64, hex, qp, or url.
	bool ReadBinaryToEncoded(const char *filename, const char *encoding, CkString &outStr);
	// Reads the entire contents of a binary file and returns it as an encoded string
	// (using an encoding such as Base64, Hex, etc.) The  encoding may be one of the
	// following strings: base64, hex, qp, or url.
	const char *readBinaryToEncoded(const char *filename, const char *encoding);

	// Reads the entire contents of a binary file and returns the data.
	bool ReadEntireFile(const char *filename, CkByteData &outBytes);

	// Reads the entire contents of a text file, interprets the bytes according to the
	// character encoding specified by  charset, and returns the text file as a string.
	bool ReadEntireTextFile(const char *filename, const char *charset, CkString &outStrFileContents);
	// Reads the entire contents of a text file, interprets the bytes according to the
	// character encoding specified by  charset, and returns the text file as a string.
	const char *readEntireTextFile(const char *filename, const char *charset);

	// Reassembles a file previously split by the SplitFile method.
	bool ReassembleFile(const char *partsDirPath, const char *partPrefix, const char *partExtension, const char *reassembledFilename);

	// Replaces all occurances of  existingString with  replacementString in a file. The character encoding,
	// such as utf-8, ansi, etc. is specified by  charset.
	int ReplaceStrings(const char *path, const char *charset, const char *existingString, const char *replacementString);

	// Sets the current working directory for the calling process to dirPath.
	bool SetCurrentDir(const char *path);

	// Sets the create date/time, the last-access date/time, and the last-modified
	// date/time for a file. For non-Windows filesystems where create times are not
	// implemented, the  createTime is ignored.
	bool SetFileTimes(const char *path, CkDateTime &create, CkDateTime &lastAccess, CkDateTime &lastModified);

	// Sets the last-modified date/time for a file.
	bool SetLastModified(const char *path, CkDateTime &lastModified);

	// Splits a file into chunks. Please refer to the example below:
	bool SplitFile(const char *fileToSplit, const char *partPrefix, const char *partExtension, int partSize, const char *destDir);

	// Deletes an entire directory tree (all files and sub-directories).
	bool TreeDelete(const char *path);

	// Opens/creates filePath, writes  fileData, and closes the file.
	bool WriteEntireFile(const char *filename, const CkByteData &fileData);

	// Opens filePath, writes  textData using the character encoding specified by  charset, and
	// closes the file. If  includedPreamble is true and the  charset is Unicode or utf-8, then the
	// BOM is included at the beginning of the file.
	bool WriteEntireTextFile(const char *filename, const char *fileData, const char *charset, bool includePreamble);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
