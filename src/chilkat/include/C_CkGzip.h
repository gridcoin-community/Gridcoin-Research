// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkGzip_H
#define _C_CkGzip_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkGzip CkGzip_Create(void);
CK_VISIBLE_PUBLIC void CkGzip_Dispose(HCkGzip handle);
CK_VISIBLE_PUBLIC void CkGzip_getComment(HCkGzip cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkGzip_putComment(HCkGzip cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkGzip_comment(HCkGzip cHandle);
CK_VISIBLE_PUBLIC void CkGzip_getDebugLogFilePath(HCkGzip cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkGzip_putDebugLogFilePath(HCkGzip cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkGzip_debugLogFilePath(HCkGzip cHandle);
CK_VISIBLE_PUBLIC void CkGzip_getExtraData(HCkGzip cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkGzip_putExtraData(HCkGzip cHandle, HCkByteData  newVal);
CK_VISIBLE_PUBLIC void CkGzip_getFilename(HCkGzip cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkGzip_putFilename(HCkGzip cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkGzip_filename(HCkGzip cHandle);
CK_VISIBLE_PUBLIC int CkGzip_getHeartbeatMs(HCkGzip cHandle);
CK_VISIBLE_PUBLIC void CkGzip_putHeartbeatMs(HCkGzip cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkGzip_getLastErrorHtml(HCkGzip cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkGzip_lastErrorHtml(HCkGzip cHandle);
CK_VISIBLE_PUBLIC void CkGzip_getLastErrorText(HCkGzip cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkGzip_lastErrorText(HCkGzip cHandle);
CK_VISIBLE_PUBLIC void CkGzip_getLastErrorXml(HCkGzip cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkGzip_lastErrorXml(HCkGzip cHandle);
CK_VISIBLE_PUBLIC void CkGzip_getLastMod(HCkGzip cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkGzip_putLastMod(HCkGzip cHandle, SYSTEMTIME *newVal);
CK_VISIBLE_PUBLIC void CkGzip_getLastModStr(HCkGzip cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkGzip_putLastModStr(HCkGzip cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkGzip_lastModStr(HCkGzip cHandle);
CK_VISIBLE_PUBLIC BOOL CkGzip_getUseCurrentDate(HCkGzip cHandle);
CK_VISIBLE_PUBLIC void CkGzip_putUseCurrentDate(HCkGzip cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkGzip_getUtf8(HCkGzip cHandle);
CK_VISIBLE_PUBLIC void CkGzip_putUtf8(HCkGzip cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkGzip_getVerboseLogging(HCkGzip cHandle);
CK_VISIBLE_PUBLIC void CkGzip_putVerboseLogging(HCkGzip cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkGzip_getVersion(HCkGzip cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkGzip_version(HCkGzip cHandle);
CK_VISIBLE_PUBLIC BOOL CkGzip_CompressFile(HCkGzip cHandle, const char *inFilename, const char *outGzipFilename);
CK_VISIBLE_PUBLIC BOOL CkGzip_CompressFile2(HCkGzip cHandle, const char *inFilename, const char *embeddedFilename, const char *outGzipFilename);
CK_VISIBLE_PUBLIC BOOL CkGzip_CompressFileToMem(HCkGzip cHandle, const char *inFilename, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkGzip_CompressMemToFile(HCkGzip cHandle, HCkByteData inData, const char *destPath);
CK_VISIBLE_PUBLIC BOOL CkGzip_CompressMemory(HCkGzip cHandle, HCkByteData inData, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkGzip_CompressString(HCkGzip cHandle, const char *inStr, const char *destCharset, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkGzip_CompressStringENC(HCkGzip cHandle, const char *inStr, const char *charset, const char *encoding, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkGzip_compressStringENC(HCkGzip cHandle, const char *inStr, const char *charset, const char *encoding);
CK_VISIBLE_PUBLIC BOOL CkGzip_CompressStringToFile(HCkGzip cHandle, const char *inStr, const char *destCharset, const char *destPath);
CK_VISIBLE_PUBLIC BOOL CkGzip_Decode(HCkGzip cHandle, const char *encodedStr, const char *encoding, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkGzip_DeflateStringENC(HCkGzip cHandle, const char *inString, const char *charsetName, const char *outputEncoding, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkGzip_deflateStringENC(HCkGzip cHandle, const char *inString, const char *charsetName, const char *outputEncoding);
CK_VISIBLE_PUBLIC BOOL CkGzip_Encode(HCkGzip cHandle, HCkByteData byteData, const char *encoding, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkGzip_encode(HCkGzip cHandle, HCkByteData byteData, const char *encoding);
CK_VISIBLE_PUBLIC BOOL CkGzip_ExamineFile(HCkGzip cHandle, const char *inGzFilename);
CK_VISIBLE_PUBLIC BOOL CkGzip_ExamineMemory(HCkGzip cHandle, HCkByteData inGzData);
CK_VISIBLE_PUBLIC HCkDateTime CkGzip_GetDt(HCkGzip cHandle);
CK_VISIBLE_PUBLIC BOOL CkGzip_InflateStringENC(HCkGzip cHandle, const char *inString, const char *convertFromCharset, const char *inputEncoding, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkGzip_inflateStringENC(HCkGzip cHandle, const char *inString, const char *convertFromCharset, const char *inputEncoding);
CK_VISIBLE_PUBLIC BOOL CkGzip_IsUnlocked(HCkGzip cHandle);
CK_VISIBLE_PUBLIC BOOL CkGzip_ReadFile(HCkGzip cHandle, const char *path, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkGzip_SaveLastError(HCkGzip cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkGzip_SetDt(HCkGzip cHandle, HCkDateTime dt);
CK_VISIBLE_PUBLIC BOOL CkGzip_UnTarGz(HCkGzip cHandle, const char *tgzFilename, const char *destDir, BOOL bNoAbsolute);
CK_VISIBLE_PUBLIC BOOL CkGzip_UncompressFile(HCkGzip cHandle, const char *srcPath, const char *destPath);
CK_VISIBLE_PUBLIC BOOL CkGzip_UncompressFileToMem(HCkGzip cHandle, const char *inFilename, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkGzip_UncompressFileToString(HCkGzip cHandle, const char *gzFilename, const char *charset, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkGzip_uncompressFileToString(HCkGzip cHandle, const char *gzFilename, const char *charset);
CK_VISIBLE_PUBLIC BOOL CkGzip_UncompressMemToFile(HCkGzip cHandle, HCkByteData inData, const char *destPath);
CK_VISIBLE_PUBLIC BOOL CkGzip_UncompressMemory(HCkGzip cHandle, HCkByteData inData, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkGzip_UncompressString(HCkGzip cHandle, HCkByteData inData, const char *inCharset, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkGzip_uncompressString(HCkGzip cHandle, HCkByteData inData, const char *inCharset);
CK_VISIBLE_PUBLIC BOOL CkGzip_UncompressStringENC(HCkGzip cHandle, const char *inStr, const char *charset, const char *encoding, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkGzip_uncompressStringENC(HCkGzip cHandle, const char *inStr, const char *charset, const char *encoding);
CK_VISIBLE_PUBLIC BOOL CkGzip_UnlockComponent(HCkGzip cHandle, const char *unlockCode);
CK_VISIBLE_PUBLIC BOOL CkGzip_WriteFile(HCkGzip cHandle, const char *path, HCkByteData binaryData);
CK_VISIBLE_PUBLIC BOOL CkGzip_XfdlToXml(HCkGzip cHandle, const char *xfldData, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkGzip_xfdlToXml(HCkGzip cHandle, const char *xfldData);
#endif
