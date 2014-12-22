// CkCompression.h: interface for the CkCompression class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkCompression_H
#define _CkCompression_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"

class CkByteData;
class CkBaseProgress;



#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkCompression
class CK_VISIBLE_PUBLIC CkCompression  : public CkMultiByteBase
{
    private:
	CkBaseProgress *m_callback;

	// Don't allow assignment or copying these objects.
	CkCompression(const CkCompression &);
	CkCompression &operator=(const CkCompression &);

    public:
	CkCompression(void);
	virtual ~CkCompression(void);

	static CkCompression *createNew(void);
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
	// Only applies to methods that compress or decompress strings. This specifies the
	// character encoding that the string should be converted to before compression.
	// Many programming languages use Unicode (2 bytes per char) for representing
	// characters. This property allows for the string to be converted to a 1-byte per
	// char encoding prior to compression.
	void get_Charset(CkString &str);
	// Only applies to methods that compress or decompress strings. This specifies the
	// character encoding that the string should be converted to before compression.
	// Many programming languages use Unicode (2 bytes per char) for representing
	// characters. This property allows for the string to be converted to a 1-byte per
	// char encoding prior to compression.
	const char *charset(void);
	// Only applies to methods that compress or decompress strings. This specifies the
	// character encoding that the string should be converted to before compression.
	// Many programming languages use Unicode (2 bytes per char) for representing
	// characters. This property allows for the string to be converted to a 1-byte per
	// char encoding prior to compression.
	void put_Charset(const char *newVal);

	// Controls the encoding expected by methods ending in "ENC", such as
	// CompressBytesENC. Valid values are "base64", "hex", "url", and
	// "quoted-printable". Compression methods ending in ENC return the binary
	// compressed data as an encoded string using this encoding. Decompress methods
	// expect the input string to be this encoding.
	void get_EncodingMode(CkString &str);
	// Controls the encoding expected by methods ending in "ENC", such as
	// CompressBytesENC. Valid values are "base64", "hex", "url", and
	// "quoted-printable". Compression methods ending in ENC return the binary
	// compressed data as an encoded string using this encoding. Decompress methods
	// expect the input string to be this encoding.
	const char *encodingMode(void);
	// Controls the encoding expected by methods ending in "ENC", such as
	// CompressBytesENC. Valid values are "base64", "hex", "url", and
	// "quoted-printable". Compression methods ending in ENC return the binary
	// compressed data as an encoded string using this encoding. Decompress methods
	// expect the input string to be this encoding.
	void put_EncodingMode(const char *newVal);

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

	// The compression algorithm to be used. Should be set to either "ppmd", "deflate",
	// "zlib", "bzip2", or "lzw".
	// 
	// Note: The PPMD compression algorithm is only available for Windows OS builds. It
	// is not yet available on non-Windows platforms.
	// 
	void get_Algorithm(CkString &str);
	// The compression algorithm to be used. Should be set to either "ppmd", "deflate",
	// "zlib", "bzip2", or "lzw".
	// 
	// Note: The PPMD compression algorithm is only available for Windows OS builds. It
	// is not yet available on non-Windows platforms.
	// 
	const char *algorithm(void);
	// The compression algorithm to be used. Should be set to either "ppmd", "deflate",
	// "zlib", "bzip2", or "lzw".
	// 
	// Note: The PPMD compression algorithm is only available for Windows OS builds. It
	// is not yet available on non-Windows platforms.
	// 
	void put_Algorithm(const char *newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Large amounts of binary byte data may be compressed in chunks by first calling
	// BeginCompressBytes, followed by 0 or more calls to MoreCompressedBytes, and
	// ending with a final call to EndCompressBytes. Each call returns 0 or more bytes
	// of compressed data which may be output to a compressed data stream (such as a
	// file, socket, etc.).
	bool BeginCompressBytes(const CkByteData &data, CkByteData &outData);

	// Large amounts of binary byte data may be compressed in chunks by first calling
	// BeginCompressBytesENC, followed by 0 or more calls to MoreCompressedBytesENC,
	// and ending with a final call to EndCompressBytesENC. Each call returns 0 or more
	// characters of compressed data (encoded as a string according to the EncodingMode
	// property setting) which may be output to a compressed data stream (such as a
	// file, socket, etc.).
	bool BeginCompressBytesENC(const CkByteData &data, CkString &outStr);
	// Large amounts of binary byte data may be compressed in chunks by first calling
	// BeginCompressBytesENC, followed by 0 or more calls to MoreCompressedBytesENC,
	// and ending with a final call to EndCompressBytesENC. Each call returns 0 or more
	// characters of compressed data (encoded as a string according to the EncodingMode
	// property setting) which may be output to a compressed data stream (such as a
	// file, socket, etc.).
	const char *beginCompressBytesENC(const CkByteData &data);

	// Large amounts of string data may be compressed in chunks by first calling
	// BeginCompressString, followed by 0 or more calls to MoreCompressedString, and
	// ending with a final call to EndCompressString. Each call returns 0 or more bytes
	// of compressed data which may be output to a compressed data stream (such as a
	// file, socket, etc.).
	bool BeginCompressString(const char *str, CkByteData &outData);

	// Large amounts of string data may be compressed in chunks by first calling
	// BeginCompressStringENC, followed by 0 or more calls to MoreCompressedStringENC,
	// and ending with a final call to EndCompressStringENC. Each call returns 0 or
	// more characters of compressed data (encoded as a string according to the
	// EncodingMode property setting) which may be output to a compressed data stream
	// (such as a file, socket, etc.).
	bool BeginCompressStringENC(const char *str, CkString &outStr);
	// Large amounts of string data may be compressed in chunks by first calling
	// BeginCompressStringENC, followed by 0 or more calls to MoreCompressedStringENC,
	// and ending with a final call to EndCompressStringENC. Each call returns 0 or
	// more characters of compressed data (encoded as a string according to the
	// EncodingMode property setting) which may be output to a compressed data stream
	// (such as a file, socket, etc.).
	const char *beginCompressStringENC(const char *str);

	// A compressed data stream may be decompressed in chunks by first calling
	// BeginDecompressBytes, followed by 0 or more calls to MoreDecompressedBytes, and
	// ending with a final call to EndDecompressBytes. Each call returns 0 or more
	// bytes of decompressed data.
	bool BeginDecompressBytes(const CkByteData &data, CkByteData &outData);

	// The input to this method is an encoded string containing compressed data. The
	// EncodingMode property should be set prior to calling this method. The input
	// string is decoded according to the EncodingMode (hex, base64, etc.) and then
	// decompressed.
	// 
	// A compressed data stream may be decompressed in chunks by first calling
	// BeginDecompressBytesENC, followed by 0 or more calls to
	// MoreDecompressedBytesENC, and ending with a final call to EndDecompressBytesENC.
	// Each call returns 0 or more bytes of decompressed data.
	// 
	bool BeginDecompressBytesENC(const char *str, CkByteData &outData);

	// A compressed data stream may be decompressed in chunks by first calling
	// BeginDecompressString, followed by 0 or more calls to MoreDecompressedString,
	// and ending with a final call to EndDecompressString. Each call returns 0 or more
	// characters of decompressed text.
	bool BeginDecompressString(const CkByteData &data, CkString &outStr);
	// A compressed data stream may be decompressed in chunks by first calling
	// BeginDecompressString, followed by 0 or more calls to MoreDecompressedString,
	// and ending with a final call to EndDecompressString. Each call returns 0 or more
	// characters of decompressed text.
	const char *beginDecompressString(const CkByteData &data);

	// The input to this method is an encoded string containing compressed data. The
	// EncodingMode property should be set prior to calling this method. The input
	// string is decoded according to the EncodingMode (hex, base64, etc.) and then
	// decompressed.
	// 
	// A compressed data stream may be decompressed in chunks by first calling
	// BeginDecompressStringENC, followed by 0 or more calls to
	// MoreDecompressedStringENC, and ending with a final call to
	// EndDecompressStringENC. Each call returns 0 or more characters of decompressed
	// text.
	// 
	bool BeginDecompressStringENC(const char *str, CkString &outStr);
	// The input to this method is an encoded string containing compressed data. The
	// EncodingMode property should be set prior to calling this method. The input
	// string is decoded according to the EncodingMode (hex, base64, etc.) and then
	// decompressed.
	// 
	// A compressed data stream may be decompressed in chunks by first calling
	// BeginDecompressStringENC, followed by 0 or more calls to
	// MoreDecompressedStringENC, and ending with a final call to
	// EndDecompressStringENC. Each call returns 0 or more characters of decompressed
	// text.
	// 
	const char *beginDecompressStringENC(const char *str);

	// Compresses byte data.
	bool CompressBytes(const CkByteData &data, CkByteData &outData);

	// Compresses bytes and returns the compressed data encoded to a string. The
	// encoding (hex, base64, etc.) is determined by the EncodingMode property setting.
	bool CompressBytesENC(const CkByteData &data, CkString &outStr);
	// Compresses bytes and returns the compressed data encoded to a string. The
	// encoding (hex, base64, etc.) is determined by the EncodingMode property setting.
	const char *compressBytesENC(const CkByteData &data);

	// Performs file-to-file compression. Files of any size may be compressed because
	// the file is compressed internally in streaming mode.
	bool CompressFile(const char *srcPath, const char *destPath);

	// Compresses a string.
	bool CompressString(const char *str, CkByteData &outData);

	// Compresses a string and returns the compressed data encoded to a string. The
	// output encoding (hex, base64, etc.) is determined by the EncodingMode property
	// setting.
	bool CompressStringENC(const char *str, CkString &outStr);
	// Compresses a string and returns the compressed data encoded to a string. The
	// output encoding (hex, base64, etc.) is determined by the EncodingMode property
	// setting.
	const char *compressStringENC(const char *str);

	// The opposite of CompressBytes.
	bool DecompressBytes(const CkByteData &data, CkByteData &outData);

	// The opposite of CompressBytesENC. encodedCompressedData contains the compressed data as an
	// encoded string (hex, base64, etc) as specified by the EncodingMode property
	// setting.
	bool DecompressBytesENC(const char *str, CkByteData &outData);

	// Performs file-to-file decompression (the opposite of CompressFile). Internally
	// the file is decompressed in streaming mode which allows files of any size to be
	// decompressed.
	bool DecompressFile(const char *srcPath, const char *destPath);

	// Takes compressed bytes, decompresses, and returns the resulting string.
	bool DecompressString(const CkByteData &data, CkString &outStr);
	// Takes compressed bytes, decompresses, and returns the resulting string.
	const char *decompressString(const CkByteData &data);

	// The opposite of CompressStringENC. encodedCompressedData contains the compressed data as an
	// encoded string (hex, base64, etc) as specified by the EncodingMode property
	// setting.
	bool DecompressStringENC(const char *str, CkString &outStr);
	// The opposite of CompressStringENC. encodedCompressedData contains the compressed data as an
	// encoded string (hex, base64, etc) as specified by the EncodingMode property
	// setting.
	const char *decompressStringENC(const char *str);

	// Must be callled to finalize a compression stream. Returns any remaining
	// (buffered) compressed data.
	// 
	// (See BeginCompressBytes)
	// 
	bool EndCompressBytes(CkByteData &outData);

	// Must be callled to finalize a compression stream. Returns any remaining
	// (buffered) compressed data.
	// 
	// (See BeginCompressBytesENC)
	// 
	bool EndCompressBytesENC(CkString &outStr);
	// Must be callled to finalize a compression stream. Returns any remaining
	// (buffered) compressed data.
	// 
	// (See BeginCompressBytesENC)
	// 
	const char *endCompressBytesENC(void);

	// Must be callled to finalize a compression stream. Returns any remaining
	// (buffered) compressed data.
	// 
	// (See BeginCompressString)
	// 
	bool EndCompressString(CkByteData &outData);

	// Must be callled to finalize a compression stream. Returns any remaining
	// (buffered) compressed data.
	// 
	// (See BeginCompressStringENC)
	// 
	bool EndCompressStringENC(CkString &outStr);
	// Must be callled to finalize a compression stream. Returns any remaining
	// (buffered) compressed data.
	// 
	// (See BeginCompressStringENC)
	// 
	const char *endCompressStringENC(void);

	// Called to finalize the decompression stream and return any remaining (buffered)
	// decompressed data.
	// 
	// (See BeginDecompressBytes)
	// 
	bool EndDecompressBytes(CkByteData &outData);

	// Called to finalize the decompression stream and return any remaining (buffered)
	// decompressed data.
	// 
	// The input to this method is an encoded string containing compressed data. The
	// EncodingMode property should be set prior to calling this method. The input
	// string is decoded according to the EncodingMode (hex, base64, etc.) and then
	// decompressed.
	// 
	// (See BeginDecompressBytesENC)
	// 
	bool EndDecompressBytesENC(CkByteData &outData);

	// Called to finalize the decompression stream and return any remaining (buffered)
	// decompressed data.
	// 
	// (See BeginDecompressString)
	// 
	bool EndDecompressString(CkString &outStr);
	// Called to finalize the decompression stream and return any remaining (buffered)
	// decompressed data.
	// 
	// (See BeginDecompressString)
	// 
	const char *endDecompressString(void);

	// Called to finalize the decompression stream and return any remaining (buffered)
	// decompressed data.
	// 
	// The input to this method is an encoded string containing compressed data. The
	// EncodingMode property should be set prior to calling this method. The input
	// string is decoded according to the EncodingMode (hex, base64, etc.) and then
	// decompressed.
	// 
	// (See BeginDecompressStringENC)
	// 
	bool EndDecompressStringENC(CkString &outStr);
	// Called to finalize the decompression stream and return any remaining (buffered)
	// decompressed data.
	// 
	// The input to this method is an encoded string containing compressed data. The
	// EncodingMode property should be set prior to calling this method. The input
	// string is decoded according to the EncodingMode (hex, base64, etc.) and then
	// decompressed.
	// 
	// (See BeginDecompressStringENC)
	// 
	const char *endDecompressStringENC(void);

	// (See BeginCompressBytes)
	bool MoreCompressBytes(const CkByteData &data, CkByteData &outData);

	// (See BeginCompressBytesENC)
	bool MoreCompressBytesENC(const CkByteData &data, CkString &outStr);
	// (See BeginCompressBytesENC)
	const char *moreCompressBytesENC(const CkByteData &data);

	// (See BeginCompressString)
	bool MoreCompressString(const char *str, CkByteData &outData);

	// (See BeginCompressStringENC)
	bool MoreCompressStringENC(const char *str, CkString &outStr);
	// (See BeginCompressStringENC)
	const char *moreCompressStringENC(const char *str);

	// (See BeginDecompressBytes)
	bool MoreDecompressBytes(const CkByteData &data, CkByteData &outData);

	// The input to this method is an encoded string containing compressed data. The
	// EncodingMode property should be set prior to calling this method. The input
	// string is decoded according to the EncodingMode (hex, base64, etc.) and then
	// decompressed.
	// 
	// (See BeginDecompressBytesENC)
	// 
	bool MoreDecompressBytesENC(const char *str, CkByteData &outData);

	// (See BeginDecompressString)
	bool MoreDecompressString(const CkByteData &data, CkString &outStr);
	// (See BeginDecompressString)
	const char *moreDecompressString(const CkByteData &data);

	// The input to this method is an encoded string containing compressed data. The
	// EncodingMode property should be set prior to calling this method. The input
	// string is decoded according to the EncodingMode (hex, base64, etc.) and then
	// decompressed.
	// 
	// (See BeginDecompressStringENC)
	// 
	bool MoreDecompressStringENC(const char *str, CkString &outStr);
	// The input to this method is an encoded string containing compressed data. The
	// EncodingMode property should be set prior to calling this method. The input
	// string is decoded according to the EncodingMode (hex, base64, etc.) and then
	// decompressed.
	// 
	// (See BeginDecompressStringENC)
	// 
	const char *moreDecompressStringENC(const char *str);

	// Unlocks the component allowing for the full functionality to be used. The
	// component may be used fully-functional for the 1st 30-days after download by
	// passing an arbitrary string to this method. If for some reason you do not
	// receive the full 30-day trial, send email to support@chilkatsoft.com for a
	// temporary unlock code w/ an explicit expiration date. Upon purchase, a permanent
	// unlock code is provided which should replace the temporary/arbitrary string
	// passed to this method.
	bool UnlockComponent(const char *unlockCode);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
