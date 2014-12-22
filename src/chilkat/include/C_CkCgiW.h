// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCgiWH
#define _C_CkCgiWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCgiW CkCgiW_Create(void);
CK_VISIBLE_PUBLIC void CkCgiW_Dispose(HCkCgiW handle);
CK_VISIBLE_PUBLIC unsigned long CkCgiW_getAsyncBytesRead(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgiW_getAsyncInProgress(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC unsigned long CkCgiW_getAsyncPostSize(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgiW_getAsyncSuccess(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC void CkCgiW_getDebugLogFilePath(HCkCgiW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCgiW_putDebugLogFilePath(HCkCgiW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCgiW_debugLogFilePath(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC int CkCgiW_getHeartbeatMs(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC void CkCgiW_putHeartbeatMs(HCkCgiW cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkCgiW_getIdleTimeoutMs(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC void CkCgiW_putIdleTimeoutMs(HCkCgiW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkCgiW_getLastErrorHtml(HCkCgiW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCgiW_lastErrorHtml(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC void CkCgiW_getLastErrorText(HCkCgiW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCgiW_lastErrorText(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC void CkCgiW_getLastErrorXml(HCkCgiW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCgiW_lastErrorXml(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC int CkCgiW_getNumParams(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC int CkCgiW_getNumUploadFiles(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC int CkCgiW_getReadChunkSize(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC void CkCgiW_putReadChunkSize(HCkCgiW cHandle, int newVal);
CK_VISIBLE_PUBLIC unsigned long CkCgiW_getSizeLimitKB(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC void CkCgiW_putSizeLimitKB(HCkCgiW cHandle, unsigned long newVal);
CK_VISIBLE_PUBLIC BOOL CkCgiW_getStreamToUploadDir(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC void CkCgiW_putStreamToUploadDir(HCkCgiW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCgiW_getUploadDir(HCkCgiW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCgiW_putUploadDir(HCkCgiW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCgiW_uploadDir(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgiW_getVerboseLogging(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC void CkCgiW_putVerboseLogging(HCkCgiW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCgiW_getVersion(HCkCgiW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCgiW_version(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC void CkCgiW_AbortAsync(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgiW_GetEnv(HCkCgiW cHandle, const wchar_t *varName, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCgiW_getEnv(HCkCgiW cHandle, const wchar_t *varName);
CK_VISIBLE_PUBLIC BOOL CkCgiW_GetParam(HCkCgiW cHandle, const wchar_t *paramName, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCgiW_getParam(HCkCgiW cHandle, const wchar_t *paramName);
CK_VISIBLE_PUBLIC BOOL CkCgiW_GetParamName(HCkCgiW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCgiW_getParamName(HCkCgiW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCgiW_GetParamValue(HCkCgiW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCgiW_getParamValue(HCkCgiW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCgiW_GetRawPostData(HCkCgiW cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCgiW_GetUploadData(HCkCgiW cHandle, int index, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCgiW_GetUploadFilename(HCkCgiW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCgiW_getUploadFilename(HCkCgiW cHandle, int index);
CK_VISIBLE_PUBLIC unsigned long CkCgiW_GetUploadSize(HCkCgiW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCgiW_IsGet(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgiW_IsHead(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgiW_IsPost(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgiW_IsUpload(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgiW_ReadRequest(HCkCgiW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgiW_SaveLastError(HCkCgiW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkCgiW_SaveNthToUploadDir(HCkCgiW cHandle, int index);
CK_VISIBLE_PUBLIC void CkCgiW_SleepMs(HCkCgiW cHandle, int millisec);
CK_VISIBLE_PUBLIC BOOL CkCgiW_TestConsumeAspUpload(HCkCgiW cHandle, const wchar_t *path);
#endif
