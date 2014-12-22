// CkSFtpFile.h: interface for the CkSFtpFile class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkSFtpFile_H
#define _CkSFtpFile_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkDateTime;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkSFtpFile
class CK_VISIBLE_PUBLIC CkSFtpFile  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkSFtpFile(const CkSFtpFile &);
	CkSFtpFile &operator=(const CkSFtpFile &);

    public:
	CkSFtpFile(void);
	virtual ~CkSFtpFile(void);

	static CkSFtpFile *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// The file creation date and time. This property is only supported by servers
	// running SFTP v4 or later.
	void get_CreateTime(SYSTEMTIME &outSysTime);

	// The same as the CreateTime property, but returns the date/time as an RFC822
	// formatted string.
	void get_CreateTimeStr(CkString &str);
	// The same as the CreateTime property, but returns the date/time as an RFC822
	// formatted string.
	const char *createTimeStr(void);

	// One of the following values:
	//   regular
	//   directory
	//   symLink
	//   special
	//   unknown
	//   socket
	//   charDevice
	//   blockDevice
	//   fifo
	void get_FileType(CkString &str);
	// One of the following values:
	//   regular
	//   directory
	//   symLink
	//   special
	//   unknown
	//   socket
	//   charDevice
	//   blockDevice
	//   fifo
	const char *fileType(void);

	// The filename (or directory name, symbolic link name, etc.)
	void get_Filename(CkString &str);
	// The filename (or directory name, symbolic link name, etc.)
	const char *filename(void);

	// The integer Group ID of the file.
	int get_Gid(void);

	// The group ownership of the file. This property is only supported by servers
	// running SFTP v4 or later.
	void get_Group(CkString &str);
	// The group ownership of the file. This property is only supported by servers
	// running SFTP v4 or later.
	const char *group(void);

	// If true, this file may only be appended. This property is only supported by
	// servers running SFTP v6 or later.
	bool get_IsAppendOnly(void);

	// If true, the file should be included in backup / archive operations. This
	// property is only supported by servers running SFTP v6 or later.
	bool get_IsArchive(void);

	// This attribute applies only to directories. This attribute means that files and
	// directory names in this directory should be compared without regard to case.
	// This property is only supported by servers running SFTP v6 or later.
	bool get_IsCaseInsensitive(void);

	// The file is stored on disk using file-system level transparent compression. This
	// flag does not affect the file data on the wire. This property is only supported
	// by servers running SFTP v6 or later.
	bool get_IsCompressed(void);

	// If true, this is a directory.
	bool get_IsDirectory(void);

	// The file is stored on disk using file-system level transparent encryption. This
	// flag does not affect the file data on the wire (for either READ or WRITE
	// requests.) This property is only supported by servers running SFTP v6 or later.
	bool get_IsEncrypted(void);

	// If true, the file SHOULD NOT be shown to user unless specifically requested.
	bool get_IsHidden(void);

	// The file cannot be deleted or renamed, no hard link can be created to this file,
	// and no data can be written to the file.
	// 
	// This bit implies a stronger level of protection than ReadOnly, the file
	// permission mask or ACLs. Typically even the superuser cannot write to immutable
	// files, and only the superuser can set or remove the bit.
	// 
	// This property is only supported by servers running SFTP v6 or later.
	// 
	bool get_IsImmutable(void);

	// If true, the file is read-only. This property is only supported by servers
	// running SFTP v6 or later.
	bool get_IsReadOnly(void);

	// true if this is a normal file (not a directory or any of the other non-file
	// types).
	bool get_IsRegular(void);

	// The file is a sparse file; this means that file blocks that have not been
	// explicitly written are not stored on disk. For example, if a client writes a
	// buffer at 10 M from the beginning of the file, the blocks between the previous
	// EOF marker and the 10 M offset would not consume physical disk space.
	// 
	// Some servers may store all files as sparse files, in which case this bit will be
	// unconditionally set. Other servers may not have a mechanism for determining if
	// the file is sparse, and so the file MAY be stored sparse even if this flag is
	// not set.
	// 
	// This property is only supported by servers running SFTP v6 or later.
	// 
	bool get_IsSparse(void);

	// true if this is a symbolic link.
	bool get_IsSymLink(void);

	// When the file is modified, the changes are written synchronously to the disk.
	// This property is only supported by servers running SFTP v6 or later.
	bool get_IsSync(void);

	// true if the file is part of the operating system. This property is only
	// supported by servers running SFTP v6 or later.
	bool get_IsSystem(void);

	// The last-access date and time.
	void get_LastAccessTime(SYSTEMTIME &outSysTime);

	// The same as the LastAccessTime property, but returns the date/time as an RFC822
	// formatted string.
	void get_LastAccessTimeStr(CkString &str);
	// The same as the LastAccessTime property, but returns the date/time as an RFC822
	// formatted string.
	const char *lastAccessTimeStr(void);

	// The last-modified date and time.
	void get_LastModifiedTime(SYSTEMTIME &outSysTime);

	// The same as the LastModifiedTime property, but returns the date/time as an
	// RFC822 formatted string.
	void get_LastModifiedTimeStr(CkString &str);
	// The same as the LastModifiedTime property, but returns the date/time as an
	// RFC822 formatted string.
	const char *lastModifiedTimeStr(void);

	// The owner of the file. This property is only supported by servers running SFTP
	// v4 or later.
	void get_Owner(CkString &str);
	// The owner of the file. This property is only supported by servers running SFTP
	// v4 or later.
	const char *owner(void);

	// The 'permissions' field contains a bit mask specifying file permissions. These
	// permissions correspond to the st_mode field of the stat structure defined by
	// POSIX [IEEE.1003-1.1996].
	// 
	// This protocol uses the following values for the symbols declared in the POSIX
	// standard.
	//        S_IRUSR  0000400 (octal)
	//        S_IWUSR  0000200
	//        S_IXUSR  0000100
	//        S_IRGRP  0000040
	//        S_IWGRP  0000020
	//        S_IXGRP  0000010
	//        S_IROTH  0000004
	//        S_IWOTH  0000002
	//        S_IXOTH  0000001
	//        S_ISUID  0004000
	//        S_ISGID  0002000
	//        S_ISVTX  0001000
	// 
	int get_Permissions(void);

	// Size of the file in bytes. If the size is too large for 32-bits, a -1 is
	// returned.
	int get_Size32(void);

	// Size of the file in bytes. If the file size is a number too large for 64 bits,
	// you have an AMAZINGLY large disk drive.
	__int64 get_Size64(void);

	// Same as Size64, but the number is returned as a string in decimal format.
	void get_SizeStr(CkString &str);
	// Same as Size64, but the number is returned as a string in decimal format.
	const char *sizeStr(void);

	// The integer User ID of the file.
	int get_Uid(void);



	// ----------------------
	// Methods
	// ----------------------
	// Returns the last-access date and time (GMT / UTC).
	// The caller is responsible for deleting the object returned by this method.
	CkDateTime *GetLastAccessDt(void);

	// Returns the last-modified date and time (GMT / UTC).
	// The caller is responsible for deleting the object returned by this method.
	CkDateTime *GetLastModifiedDt(void);

	// Returns the file creation date and time (GMT / UTC). This method is only
	// supported by servers running SFTP v4 or later.
	// The caller is responsible for deleting the object returned by this method.
	CkDateTime *GetCreateDt(void);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
