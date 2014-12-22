// CkUnixCompress.h: interface for the CkUnixCompress class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkUnixCompress_H
#define _CkUnixCompress_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;
class CkBaseProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkUnixCompress
class CK_VISIBLE_PUBLIC CkUnixCompress  : public CkMultiByteBase
{
    private:
	CkBaseProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkUnixCompress(const CkUnixCompress &);
	CkUnixCompress &operator=(const CkUnixCompress &);

    public:
	CkUnixCompress(void);
	virtual ~CkUnixCompress(void);

	static CkUnixCompress *createNew(void);
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



	// ----------------------
	// Methods
	// ----------------------
	// Compresses a file to create a UNIX compressed file (.Z). This compression uses
	// the LZW (Lempel-Ziv-Welch) compression algorithm.
	bool CompressFile(const char *inFilename, const char *destPath);

	// Unix compresses a file to an in-memory image of a .Z file. (This compression
	// uses the LZW (Lempel-Ziv-Welch) compression algorithm.)
	bool CompressFileToMem(const char *inFilename, CkByteData &outData);

	// Unix compresses and creates a .Z file from in-memory data. (This compression
	// uses the LZW (Lempel-Ziv-Welch) compression algorithm.)
	bool CompressMemToFile(const CkByteData &inData, const char *destPath);

	// Compresses in-memory data to an in-memory image of a .Z file. (This compression
	// uses the LZW (Lempel-Ziv-Welch) compression algorithm.)
	bool CompressMemory(const CkByteData &inData, CkByteData &outData);

	// Compresses a string to an in-memory image of a .Z file. Prior to compression,
	// the string is converted to the character encoding specified by  charset. The  charset
	// can be any one of a large number of character encodings, such as "utf-8",
	// "iso-8859-1", "Windows-1252", "ucs-2", etc.
	bool CompressString(const char *inStr, const char *charset, CkByteData &outBytes);

	// Unix compresses and creates a .Z file from string data. The  charset specifies the
	// character encoding used for the byte representation of the characters when
	// compressed. The  charset can be any one of a large number of character encodings,
	// such as "utf-8", "iso-8859-1", "Windows-1252", "ucs-2", etc.
	bool CompressStringToFile(const char *inStr, const char *charset, const char *destPath);

	// Returns true if the component has been unlocked. (For ActiveX, returns 1 if
	// unlocked.)
	bool IsUnlocked(void);

	// Unpacks a .tar.Z file. The decompress and untar occur in streaming mode. There
	// are no temporary files and the memory footprint is constant (and small) while
	// untarring.
	bool UnTarZ(const char *zFilename, const char *destDir, bool bNoAbsolute);

	// Uncompresses a .Z file. (This compression uses the LZW (Lempel-Ziv-Welch)
	// compression algorithm.)
	bool UncompressFile(const char *inFilename, const char *destPath);

	// Uncompresses a .Z file directly to memory. (This compression uses the LZW
	// (Lempel-Ziv-Welch) compression algorithm.)
	bool UncompressFileToMem(const char *inFilename, CkByteData &outData);

	// Uncompresses a .Z file that contains a text file. The contents of the text file
	// are returned as a string. The character encoding of the text file is specified
	// by  charset. Typical charsets are "iso-8859-1", "utf-8", "windows-1252",
	// "shift_JIS", "big5", etc.
	bool UncompressFileToString(const char *inFilename, const char *inCharset, CkString &outStr);
	// Uncompresses a .Z file that contains a text file. The contents of the text file
	// are returned as a string. The character encoding of the text file is specified
	// by  charset. Typical charsets are "iso-8859-1", "utf-8", "windows-1252",
	// "shift_JIS", "big5", etc.
	const char *uncompressFileToString(const char *inFilename, const char *inCharset);

	// Uncompresses from an in-memory image of a .Z file to a file. (This compression
	// uses the LZW (Lempel-Ziv-Welch) compression algorithm.)
	bool UncompressMemToFile(const CkByteData &inData, const char *destPath);

	// Uncompresses from an in-memory image of a .Z file directly into memory. (This
	// compression uses the LZW (Lempel-Ziv-Welch) compression algorithm.)
	bool UncompressMemory(const CkByteData &inData, CkByteData &outData);

	// Uncompresses from an in-memory image of a .Z file directly to a string. The  charset
	// specifies the character encoding used to interpret the bytes resulting from the
	// decompression. The  charset can be any one of a large number of character encodings,
	// such as "utf-8", "iso-8859-1", "Windows-1252", "ucs-2", etc.
	bool UncompressString(const CkByteData &inData, const char *inCharset, CkString &outStr);
	// Uncompresses from an in-memory image of a .Z file directly to a string. The  charset
	// specifies the character encoding used to interpret the bytes resulting from the
	// decompression. The  charset can be any one of a large number of character encodings,
	// such as "utf-8", "iso-8859-1", "Windows-1252", "ucs-2", etc.
	const char *uncompressString(const CkByteData &inData, const char *inCharset);

	// Unlocks the component allowing for the full functionality to be used.
	bool UnlockComponent(const char *unlockCode);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
