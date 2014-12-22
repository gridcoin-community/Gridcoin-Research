// CkTar.h: interface for the CkTar class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkTar_H
#define _CkTar_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;
class CkTarProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkTar
class CK_VISIBLE_PUBLIC CkTar  : public CkMultiByteBase
{
    private:
	CkTarProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkTar(const CkTar &);
	CkTar &operator=(const CkTar &);

    public:
	CkTar(void);
	virtual ~CkTar(void);

	static CkTar *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	CkTarProgress *get_EventCallbackObject(void) const;
	void put_EventCallbackObject(CkTarProgress *progress);


	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// Character encoding to be used when interpreting filenames within .tar archives
	// for untar operations. The default is "utf-8", and this is typically not changed.
	// (The WriteTar methods always uses utf-8 to store filenames within the TAR
	// archive.)
	void get_Charset(CkString &str);
	// Character encoding to be used when interpreting filenames within .tar archives
	// for untar operations. The default is "utf-8", and this is typically not changed.
	// (The WriteTar methods always uses utf-8 to store filenames within the TAR
	// archive.)
	const char *charset(void);
	// Character encoding to be used when interpreting filenames within .tar archives
	// for untar operations. The default is "utf-8", and this is typically not changed.
	// (The WriteTar methods always uses utf-8 to store filenames within the TAR
	// archive.)
	void put_Charset(const char *newVal);

	// The directory permissions to used in WriteTar* methods. The default is octal
	// 0755. This is the value to be stored in the "mode" field of each TAR header for
	// a directory entries.
	int get_DirMode(void);
	// The directory permissions to used in WriteTar* methods. The default is octal
	// 0755. This is the value to be stored in the "mode" field of each TAR header for
	// a directory entries.
	void put_DirMode(int newVal);

	// A prefix to be added to each file's path within the TAR archive as it is being
	// created. For example, if this property is set to the string "subdir1", then
	// "subdir1/" will be prepended to each file's path within the TAR.
	void get_DirPrefix(CkString &str);
	// A prefix to be added to each file's path within the TAR archive as it is being
	// created. For example, if this property is set to the string "subdir1", then
	// "subdir1/" will be prepended to each file's path within the TAR.
	const char *dirPrefix(void);
	// A prefix to be added to each file's path within the TAR archive as it is being
	// created. For example, if this property is set to the string "subdir1", then
	// "subdir1/" will be prepended to each file's path within the TAR.
	void put_DirPrefix(const char *newVal);

	// The file permissions to used in WriteTar* methods. The default is octal 0644.
	// This is the value to be stored in the "mode" field of each TAR header for a file
	// entries.
	int get_FileMode(void);
	// The file permissions to used in WriteTar* methods. The default is octal 0644.
	// This is the value to be stored in the "mode" field of each TAR header for a file
	// entries.
	void put_FileMode(int newVal);

	// The default numerical GID to be stored in each TAR header when writing TAR
	// archives. The default value is 1000.
	int get_GroupId(void);
	// The default numerical GID to be stored in each TAR header when writing TAR
	// archives. The default value is 1000.
	void put_GroupId(int newVal);

	// The default group name to be stored in each TAR header when writing TAR
	// archives. The default value is the logged-on username of the application's
	// process.
	void get_GroupName(CkString &str);
	// The default group name to be stored in each TAR header when writing TAR
	// archives. The default value is the logged-on username of the application's
	// process.
	const char *groupName(void);
	// The default group name to be stored in each TAR header when writing TAR
	// archives. The default value is the logged-on username of the application's
	// process.
	void put_GroupName(const char *newVal);

	// This is the number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any TAR operation prior to
	// completion. If HeartbeatMs is 0, no AbortCheck event callbacks will occur.
	int get_HeartbeatMs(void);
	// This is the number of milliseconds between each AbortCheck event callback. The
	// AbortCheck callback allows an application to abort any TAR operation prior to
	// completion. If HeartbeatMs is 0, no AbortCheck event callbacks will occur.
	void put_HeartbeatMs(int newVal);

	// If true, then absolute paths are converted to relative paths by removing the
	// leading "/" or "\" character when untarring. This protects your system from
	// unknowingly untarring files into important system directories, such as
	// C:\Windows\system32. The default value is true.
	bool get_NoAbsolutePaths(void);
	// If true, then absolute paths are converted to relative paths by removing the
	// leading "/" or "\" character when untarring. This protects your system from
	// unknowingly untarring files into important system directories, such as
	// C:\Windows\system32. The default value is true.
	void put_NoAbsolutePaths(bool newVal);

	// The total number of directory roots set by calling AddDirRoot (i.e. the number
	// of times AddDirRoot was called by the application). A TAR archive is created by
	// calling AddDirRoot for one or more directory tree roots, followed by a single
	// call to WriteTar (or WriteTarBz2, WriteTarGz, WriteTarZ). This allows for TAR
	// archives containing multiple directory trees to be created.
	int get_NumDirRoots(void);

	// The file permissions to used in WriteTar* methods for shell script files (.sh,
	// .csh, .bash, .bsh). The default is octal 0755. This is the value to be stored in
	// the "mode" field of each TAR header for a file entries.
	int get_ScriptFileMode(void);
	// The file permissions to used in WriteTar* methods for shell script files (.sh,
	// .csh, .bash, .bsh). The default is octal 0755. This is the value to be stored in
	// the "mode" field of each TAR header for a file entries.
	void put_ScriptFileMode(int newVal);

	// Determines whether pattern matching is case-sensitive if the UntarMatchPattern
	// is used. The default value is false.
	bool get_UntarCaseSensitive(void);
	// Determines whether pattern matching is case-sensitive if the UntarMatchPattern
	// is used. The default value is false.
	void put_UntarCaseSensitive(bool newVal);

	// Similar to the VerboseLogging property. If set to true, then information about
	// each file/directory extracted in an untar method call is logged to LastErrorText
	// (or LastErrorXml / LastErrorHtml). The default value is false.
	bool get_UntarDebugLog(void);
	// Similar to the VerboseLogging property. If set to true, then information about
	// each file/directory extracted in an untar method call is logged to LastErrorText
	// (or LastErrorXml / LastErrorHtml). The default value is false.
	void put_UntarDebugLog(bool newVal);

	// If true, then discard all path information when untarring. This causes all
	// files to be untarred into a single directory. The default value is false.
	bool get_UntarDiscardPaths(void);
	// If true, then discard all path information when untarring. This causes all
	// files to be untarred into a single directory. The default value is false.
	void put_UntarDiscardPaths(bool newVal);

	// The directory path where files are extracted when untarring. The default value
	// is ".", meaning that the current working directory of the calling process is
	// used. If UntarDiscardPaths is set, then all files are untarred into this
	// directory. Otherwise, the untar operation will re-create a directory tree rooted
	// in this directory.
	void get_UntarFromDir(CkString &str);
	// The directory path where files are extracted when untarring. The default value
	// is ".", meaning that the current working directory of the calling process is
	// used. If UntarDiscardPaths is set, then all files are untarred into this
	// directory. Otherwise, the untar operation will re-create a directory tree rooted
	// in this directory.
	const char *untarFromDir(void);
	// The directory path where files are extracted when untarring. The default value
	// is ".", meaning that the current working directory of the calling process is
	// used. If UntarDiscardPaths is set, then all files are untarred into this
	// directory. Otherwise, the untar operation will re-create a directory tree rooted
	// in this directory.
	void put_UntarFromDir(const char *newVal);

	// Used to untar only files and directories that match this pattern. The asterisk
	// character represents 0 or more of any character. For example, setting this
	// pattern to "*.txt" causes all .txt files to be extracted. The default value is
	// an empty string, indicating that all files are to be extracted.
	void get_UntarMatchPattern(CkString &str);
	// Used to untar only files and directories that match this pattern. The asterisk
	// character represents 0 or more of any character. For example, setting this
	// pattern to "*.txt" causes all .txt files to be extracted. The default value is
	// an empty string, indicating that all files are to be extracted.
	const char *untarMatchPattern(void);
	// Used to untar only files and directories that match this pattern. The asterisk
	// character represents 0 or more of any character. For example, setting this
	// pattern to "*.txt" causes all .txt files to be extracted. The default value is
	// an empty string, indicating that all files are to be extracted.
	void put_UntarMatchPattern(const char *newVal);

	// Limits the number of files extracted during an untar to this count. The default
	// value is 0 to indicate no maximum. To untar a single file, one might set the
	// UntarMatchPattern such that it will match only the file to be extracted, and
	// also set UntarMaxCount equal to 1. This causes an untar to scan the TAR archive
	// until it finds the matching file, extract it, and then return.
	int get_UntarMaxCount(void);
	// Limits the number of files extracted during an untar to this count. The default
	// value is 0 to indicate no maximum. To untar a single file, one might set the
	// UntarMatchPattern such that it will match only the file to be extracted, and
	// also set UntarMaxCount equal to 1. This causes an untar to scan the TAR archive
	// until it finds the matching file, extract it, and then return.
	void put_UntarMaxCount(int newVal);

	// The default numerical UID to be stored in each TAR header when writing TAR
	// archives. The default value is 1000.
	int get_UserId(void);
	// The default numerical UID to be stored in each TAR header when writing TAR
	// archives. The default value is 1000.
	void put_UserId(int newVal);

	// The default user name to be stored in each TAR header when writing TAR archives.
	// The default value is the logged-on username of the application's process.
	void get_UserName(CkString &str);
	// The default user name to be stored in each TAR header when writing TAR archives.
	// The default value is the logged-on username of the application's process.
	const char *userName(void);
	// The default user name to be stored in each TAR header when writing TAR archives.
	// The default value is the logged-on username of the application's process.
	void put_UserName(const char *newVal);

	// The TAR format to use when writing a TAR archive. Valid values are "gnu", "pax",
	// and "ustar". The default value is "gnu".
	void get_WriteFormat(CkString &str);
	// The TAR format to use when writing a TAR archive. Valid values are "gnu", "pax",
	// and "ustar". The default value is "gnu".
	const char *writeFormat(void);
	// The TAR format to use when writing a TAR archive. Valid values are "gnu", "pax",
	// and "ustar". The default value is "gnu".
	void put_WriteFormat(const char *newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Adds a directory tree to be included in the next call to one of the WriteTar*
	// methods. To include multiple directory trees in a .tar, call AddDirRoot multiple
	// times followed by a single call to WriteTar.
	bool AddDirRoot(const char *dirPath);

	// Returns the value of the Nth directory root. For example, if an application
	// calls AddDirRoot twice, then the NumDirRoots property would have a value of 2,
	// and GetDirRoot(0) would return the path passed to AddDirRoot in the 1st call,
	// and GetDirRoot(1) would return the directory path in the 2nd call to AddDirRoot.
	bool GetDirRoot(int index, CkString &outStr);
	// Returns the value of the Nth directory root. For example, if an application
	// calls AddDirRoot twice, then the NumDirRoots property would have a value of 2,
	// and GetDirRoot(0) would return the path passed to AddDirRoot in the 1st call,
	// and GetDirRoot(1) would return the directory path in the 2nd call to AddDirRoot.
	const char *getDirRoot(int index);
	// Returns the value of the Nth directory root. For example, if an application
	// calls AddDirRoot twice, then the NumDirRoots property would have a value of 2,
	// and GetDirRoot(0) would return the path passed to AddDirRoot in the 1st call,
	// and GetDirRoot(1) would return the directory path in the 2nd call to AddDirRoot.
	const char *dirRoot(int index);

	// Scans a TAR archive and returns XML detailing the files and directories found
	// within the TAR.
	bool ListXml(const char *tarPath, CkString &outStr);
	// Scans a TAR archive and returns XML detailing the files and directories found
	// within the TAR.
	const char *listXml(const char *tarPath);

	// Unlocks the component allowing for the full functionality to be used. If this
	// method unexpectedly returns false, examine the contents of the LastErrorText
	// property to determine the reason for failure.
	bool UnlockComponent(const char *unlockCode);

	// Extracts the files and directories from a TAR archive, reconstructing the
	// directory tree(s) in the local filesystem. The files are extracted to the
	// directory specified by the UntarFromDir property. Returns the number of files
	// and directories extracted, or -1 for failure.
	int Untar(const char *tarPath);

	// Extracts the files and directories from a tar.bz2 (or tar.bzip2) archive,
	// reconstructing the directory tree(s) in the local filesystem. The files are
	// extracted to the directory specified by the UntarFromDir property.
	bool UntarBz2(const char *tarPath);

	// Memory-to-memory untar. The first file matching the UntarMatchPattern property
	// is extracted and returned.
	bool UntarFirstMatchingToMemory(const CkByteData &tarFileBytes, const char *matchPattern, CkByteData &outBytes);

	// Extracts the files and directories from an in-memory TAR archive, reconstructing
	// the directory tree(s) in the local filesystem. The files are extracted to the
	// directory specified by the UntarFromDir property. Returns the number of files
	// and directories extracted, or -1 for failure.
	int UntarFromMemory(const CkByteData &tarFileBytes);

	// Extracts the files and directories from a tar.gz (or tar.gzip) archive,
	// reconstructing the directory tree(s) in the local filesystem. The files are
	// extracted to the directory specified by the UntarFromDir property.
	bool UntarGz(const char *tarPath);

	// Extracts the files and directories from a tar.Z archive, reconstructing the
	// directory tree(s) in the local filesystem. The files are extracted to the
	// directory specified by the UntarFromDir property.
	bool UntarZ(const char *tarPath);

	// Verifies that a TAR archive is valid. This method opens the TAR archive and
	// scans the entire file by walking the TAR headers. Returns true if no errors
	// were found. Otherwise returns false.
	bool VerifyTar(const char *tarPath);

	// Writes a TAR archive. The directory trees previously added by calling AddDirRoot
	// one or more times are included in the output TAR archive.
	bool WriteTar(const char *tarPath);

	// Writes a .tar.bz2 compressed TAR archive. The directory trees previously added
	// by calling AddDirRoot one or more times are included in the output file.
	bool WriteTarBz2(const char *bz2Path);

	// Writes a .tar.gz (also known as .tgz) compressed TAR archive. The directory
	// trees previously added by calling AddDirRoot one or more times are included in
	// the output file.
	bool WriteTarGz(const char *gzPath);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
