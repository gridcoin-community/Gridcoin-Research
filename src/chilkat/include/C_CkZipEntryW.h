// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkZipEntryWH
#define _C_CkZipEntryWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkZipEntryW CkZipEntryW_Create(void);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipEntryW_Create2(BOOL bCallbackOwned);
CK_VISIBLE_PUBLIC void CkZipEntryW_Dispose(HCkZipEntryW handle);
CK_VISIBLE_PUBLIC void CkZipEntryW_getComment(HCkZipEntryW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipEntryW_putComment(HCkZipEntryW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_comment(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC unsigned long CkZipEntryW_getCompressedLength(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC __int64 CkZipEntryW_getCompressedLength64(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_getCompressedLengthStr(HCkZipEntryW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_compressedLengthStr(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC int CkZipEntryW_getCompressionLevel(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_putCompressionLevel(HCkZipEntryW cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkZipEntryW_getCompressionMethod(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_putCompressionMethod(HCkZipEntryW cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkZipEntryW_getCrc(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_getDebugLogFilePath(HCkZipEntryW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipEntryW_putDebugLogFilePath(HCkZipEntryW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_debugLogFilePath(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC int CkZipEntryW_getEntryID(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC int CkZipEntryW_getEntryType(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_getFileDateTime(HCkZipEntryW cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkZipEntryW_putFileDateTime(HCkZipEntryW cHandle, SYSTEMTIME *newVal);
CK_VISIBLE_PUBLIC void CkZipEntryW_getFileDateTimeStr(HCkZipEntryW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipEntryW_putFileDateTimeStr(HCkZipEntryW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_fileDateTimeStr(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_getFileName(HCkZipEntryW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipEntryW_putFileName(HCkZipEntryW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_fileName(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_getFileNameHex(HCkZipEntryW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_fileNameHex(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC int CkZipEntryW_getHeartbeatMs(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_putHeartbeatMs(HCkZipEntryW cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_getIsDirectory(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_getLastErrorHtml(HCkZipEntryW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_lastErrorHtml(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_getLastErrorText(HCkZipEntryW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_lastErrorText(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_getLastErrorXml(HCkZipEntryW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_lastErrorXml(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_getTextFlag(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_putTextFlag(HCkZipEntryW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC unsigned long CkZipEntryW_getUncompressedLength(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC __int64 CkZipEntryW_getUncompressedLength64(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_getUncompressedLengthStr(HCkZipEntryW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_uncompressedLengthStr(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_getVerboseLogging(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC void CkZipEntryW_putVerboseLogging(HCkZipEntryW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkZipEntryW_getVersion(HCkZipEntryW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_version(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_AppendData(HCkZipEntryW cHandle, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_AppendString(HCkZipEntryW cHandle, const wchar_t *strContent, const wchar_t *charset);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_Copy(HCkZipEntryW cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_CopyToBase64(HCkZipEntryW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_copyToBase64(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_CopyToHex(HCkZipEntryW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_copyToHex(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_Extract(HCkZipEntryW cHandle, const wchar_t *dirPath);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_ExtractInto(HCkZipEntryW cHandle, const wchar_t *dirPath);
CK_VISIBLE_PUBLIC HCkDateTimeW CkZipEntryW_GetDt(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_Inflate(HCkZipEntryW cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipEntryW_NextEntry(HCkZipEntryW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_ReplaceData(HCkZipEntryW cHandle, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_ReplaceString(HCkZipEntryW cHandle, const wchar_t *strContent, const wchar_t *charset);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_SaveLastError(HCkZipEntryW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC void CkZipEntryW_SetDt(HCkZipEntryW cHandle, HCkDateTimeW dt);
CK_VISIBLE_PUBLIC BOOL CkZipEntryW_UnzipToString(HCkZipEntryW cHandle, int lineEndingBehavior, const wchar_t *srcCharset, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkZipEntryW_unzipToString(HCkZipEntryW cHandle, int lineEndingBehavior, const wchar_t *srcCharset);
#endif
