// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkGzipWH
#define _C_CkGzipWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkGzipW CkGzipW_Create(void);
CK_VISIBLE_PUBLIC HCkGzipW CkGzipW_Create2(BOOL bCallbackOwned);
CK_VISIBLE_PUBLIC void CkGzipW_Dispose(HCkGzipW handle);
CK_VISIBLE_PUBLIC void CkGzipW_getComment(HCkGzipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkGzipW_putComment(HCkGzipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_comment(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC void CkGzipW_getDebugLogFilePath(HCkGzipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkGzipW_putDebugLogFilePath(HCkGzipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_debugLogFilePath(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC void CkGzipW_getExtraData(HCkGzipW cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkGzipW_putExtraData(HCkGzipW cHandle, HCkByteData  newVal);
CK_VISIBLE_PUBLIC void CkGzipW_getFilename(HCkGzipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkGzipW_putFilename(HCkGzipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_filename(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC int CkGzipW_getHeartbeatMs(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC void CkGzipW_putHeartbeatMs(HCkGzipW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkGzipW_getLastErrorHtml(HCkGzipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_lastErrorHtml(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC void CkGzipW_getLastErrorText(HCkGzipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_lastErrorText(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC void CkGzipW_getLastErrorXml(HCkGzipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_lastErrorXml(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC void CkGzipW_getLastMod(HCkGzipW cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkGzipW_putLastMod(HCkGzipW cHandle, SYSTEMTIME *newVal);
CK_VISIBLE_PUBLIC void CkGzipW_getLastModStr(HCkGzipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkGzipW_putLastModStr(HCkGzipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_lastModStr(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC BOOL CkGzipW_getUseCurrentDate(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC void CkGzipW_putUseCurrentDate(HCkGzipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkGzipW_getVerboseLogging(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC void CkGzipW_putVerboseLogging(HCkGzipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkGzipW_getVersion(HCkGzipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_version(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC BOOL CkGzipW_CompressFile(HCkGzipW cHandle, const wchar_t *inFilename, const wchar_t *outGzipFilename);
CK_VISIBLE_PUBLIC BOOL CkGzipW_CompressFile2(HCkGzipW cHandle, const wchar_t *inFilename, const wchar_t *embeddedFilename, const wchar_t *outGzipFilename);
CK_VISIBLE_PUBLIC BOOL CkGzipW_CompressFileToMem(HCkGzipW cHandle, const wchar_t *inFilename, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkGzipW_CompressMemToFile(HCkGzipW cHandle, HCkByteData inData, const wchar_t *destPath);
CK_VISIBLE_PUBLIC BOOL CkGzipW_CompressMemory(HCkGzipW cHandle, HCkByteData inData, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkGzipW_CompressString(HCkGzipW cHandle, const wchar_t *inStr, const wchar_t *destCharset, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkGzipW_CompressStringENC(HCkGzipW cHandle, const wchar_t *inStr, const wchar_t *charset, const wchar_t *encoding, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_compressStringENC(HCkGzipW cHandle, const wchar_t *inStr, const wchar_t *charset, const wchar_t *encoding);
CK_VISIBLE_PUBLIC BOOL CkGzipW_CompressStringToFile(HCkGzipW cHandle, const wchar_t *inStr, const wchar_t *destCharset, const wchar_t *destPath);
CK_VISIBLE_PUBLIC BOOL CkGzipW_Decode(HCkGzipW cHandle, const wchar_t *encodedStr, const wchar_t *encoding, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkGzipW_DeflateStringENC(HCkGzipW cHandle, const wchar_t *inString, const wchar_t *charsetName, const wchar_t *outputEncoding, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_deflateStringENC(HCkGzipW cHandle, const wchar_t *inString, const wchar_t *charsetName, const wchar_t *outputEncoding);
CK_VISIBLE_PUBLIC BOOL CkGzipW_Encode(HCkGzipW cHandle, HCkByteData byteData, const wchar_t *encoding, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_encode(HCkGzipW cHandle, HCkByteData byteData, const wchar_t *encoding);
CK_VISIBLE_PUBLIC BOOL CkGzipW_ExamineFile(HCkGzipW cHandle, const wchar_t *inGzFilename);
CK_VISIBLE_PUBLIC BOOL CkGzipW_ExamineMemory(HCkGzipW cHandle, HCkByteData inGzData);
CK_VISIBLE_PUBLIC HCkDateTimeW CkGzipW_GetDt(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC BOOL CkGzipW_InflateStringENC(HCkGzipW cHandle, const wchar_t *inString, const wchar_t *convertFromCharset, const wchar_t *inputEncoding, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_inflateStringENC(HCkGzipW cHandle, const wchar_t *inString, const wchar_t *convertFromCharset, const wchar_t *inputEncoding);
CK_VISIBLE_PUBLIC BOOL CkGzipW_IsUnlocked(HCkGzipW cHandle);
CK_VISIBLE_PUBLIC BOOL CkGzipW_ReadFile(HCkGzipW cHandle, const wchar_t *path, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkGzipW_SaveLastError(HCkGzipW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkGzipW_SetDt(HCkGzipW cHandle, HCkDateTimeW dt);
CK_VISIBLE_PUBLIC BOOL CkGzipW_UnTarGz(HCkGzipW cHandle, const wchar_t *tgzFilename, const wchar_t *destDir, BOOL bNoAbsolute);
CK_VISIBLE_PUBLIC BOOL CkGzipW_UncompressFile(HCkGzipW cHandle, const wchar_t *srcPath, const wchar_t *destPath);
CK_VISIBLE_PUBLIC BOOL CkGzipW_UncompressFileToMem(HCkGzipW cHandle, const wchar_t *inFilename, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkGzipW_UncompressFileToString(HCkGzipW cHandle, const wchar_t *gzFilename, const wchar_t *charset, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_uncompressFileToString(HCkGzipW cHandle, const wchar_t *gzFilename, const wchar_t *charset);
CK_VISIBLE_PUBLIC BOOL CkGzipW_UncompressMemToFile(HCkGzipW cHandle, HCkByteData inData, const wchar_t *destPath);
CK_VISIBLE_PUBLIC BOOL CkGzipW_UncompressMemory(HCkGzipW cHandle, HCkByteData inData, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkGzipW_UncompressString(HCkGzipW cHandle, HCkByteData inData, const wchar_t *inCharset, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_uncompressString(HCkGzipW cHandle, HCkByteData inData, const wchar_t *inCharset);
CK_VISIBLE_PUBLIC BOOL CkGzipW_UncompressStringENC(HCkGzipW cHandle, const wchar_t *inStr, const wchar_t *charset, const wchar_t *encoding, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_uncompressStringENC(HCkGzipW cHandle, const wchar_t *inStr, const wchar_t *charset, const wchar_t *encoding);
CK_VISIBLE_PUBLIC BOOL CkGzipW_UnlockComponent(HCkGzipW cHandle, const wchar_t *unlockCode);
CK_VISIBLE_PUBLIC BOOL CkGzipW_WriteFile(HCkGzipW cHandle, const wchar_t *path, HCkByteData binaryData);
CK_VISIBLE_PUBLIC BOOL CkGzipW_XfdlToXml(HCkGzipW cHandle, const wchar_t *xfldData, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkGzipW_xfdlToXml(HCkGzipW cHandle, const wchar_t *xfldData);
#endif
