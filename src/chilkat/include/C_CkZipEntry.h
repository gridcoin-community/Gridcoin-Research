// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkZipEntry_H
#define _C_CkZipEntry_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkZipEntry CkZipEntry_Create(void);
CK_VISIBLE_PUBLIC void CkZipEntry_Dispose(HCkZipEntry handle);
CK_VISIBLE_PUBLIC void CkZipEntry_getComment(HCkZipEntry cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipEntry_putComment(HCkZipEntry cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkZipEntry_comment(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC unsigned long CkZipEntry_getCompressedLength(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC __int64 CkZipEntry_getCompressedLength64(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_getCompressedLengthStr(HCkZipEntry cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkZipEntry_compressedLengthStr(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC int CkZipEntry_getCompressionLevel(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_putCompressionLevel(HCkZipEntry cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkZipEntry_getCompressionMethod(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_putCompressionMethod(HCkZipEntry cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkZipEntry_getCrc(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_getDebugLogFilePath(HCkZipEntry cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipEntry_putDebugLogFilePath(HCkZipEntry cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkZipEntry_debugLogFilePath(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC int CkZipEntry_getEntryID(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC int CkZipEntry_getEntryType(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_getFileDateTime(HCkZipEntry cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkZipEntry_putFileDateTime(HCkZipEntry cHandle, SYSTEMTIME *newVal);
CK_VISIBLE_PUBLIC void CkZipEntry_getFileDateTimeStr(HCkZipEntry cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipEntry_putFileDateTimeStr(HCkZipEntry cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkZipEntry_fileDateTimeStr(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_getFileName(HCkZipEntry cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipEntry_putFileName(HCkZipEntry cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkZipEntry_fileName(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_getFileNameHex(HCkZipEntry cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkZipEntry_fileNameHex(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC int CkZipEntry_getHeartbeatMs(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_putHeartbeatMs(HCkZipEntry cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_getIsDirectory(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_getLastErrorHtml(HCkZipEntry cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkZipEntry_lastErrorHtml(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_getLastErrorText(HCkZipEntry cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkZipEntry_lastErrorText(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_getLastErrorXml(HCkZipEntry cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkZipEntry_lastErrorXml(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_getTextFlag(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_putTextFlag(HCkZipEntry cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC unsigned long CkZipEntry_getUncompressedLength(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC __int64 CkZipEntry_getUncompressedLength64(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_getUncompressedLengthStr(HCkZipEntry cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkZipEntry_uncompressedLengthStr(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_getUtf8(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_putUtf8(HCkZipEntry cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_getVerboseLogging(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC void CkZipEntry_putVerboseLogging(HCkZipEntry cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkZipEntry_getVersion(HCkZipEntry cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkZipEntry_version(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_AppendData(HCkZipEntry cHandle, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_AppendString(HCkZipEntry cHandle, const char *strContent, const char *charset);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_Copy(HCkZipEntry cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_CopyToBase64(HCkZipEntry cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkZipEntry_copyToBase64(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_CopyToHex(HCkZipEntry cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkZipEntry_copyToHex(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_Extract(HCkZipEntry cHandle, const char *dirPath);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_ExtractInto(HCkZipEntry cHandle, const char *dirPath);
CK_VISIBLE_PUBLIC HCkDateTime CkZipEntry_GetDt(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_Inflate(HCkZipEntry cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC HCkZipEntry CkZipEntry_NextEntry(HCkZipEntry cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_ReplaceData(HCkZipEntry cHandle, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_ReplaceString(HCkZipEntry cHandle, const char *strContent, const char *charset);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_SaveLastError(HCkZipEntry cHandle, const char *path);
CK_VISIBLE_PUBLIC void CkZipEntry_SetDt(HCkZipEntry cHandle, HCkDateTime dt);
CK_VISIBLE_PUBLIC BOOL CkZipEntry_UnzipToString(HCkZipEntry cHandle, int lineEndingBehavior, const char *srcCharset, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkZipEntry_unzipToString(HCkZipEntry cHandle, int lineEndingBehavior, const char *srcCharset);
#endif
