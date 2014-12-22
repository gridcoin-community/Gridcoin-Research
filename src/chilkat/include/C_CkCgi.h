// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCgi_H
#define _C_CkCgi_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCgi CkCgi_Create(void);
CK_VISIBLE_PUBLIC void CkCgi_Dispose(HCkCgi handle);
CK_VISIBLE_PUBLIC unsigned long CkCgi_getAsyncBytesRead(HCkCgi cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgi_getAsyncInProgress(HCkCgi cHandle);
CK_VISIBLE_PUBLIC unsigned long CkCgi_getAsyncPostSize(HCkCgi cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgi_getAsyncSuccess(HCkCgi cHandle);
CK_VISIBLE_PUBLIC void CkCgi_getDebugLogFilePath(HCkCgi cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCgi_putDebugLogFilePath(HCkCgi cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCgi_debugLogFilePath(HCkCgi cHandle);
CK_VISIBLE_PUBLIC int CkCgi_getHeartbeatMs(HCkCgi cHandle);
CK_VISIBLE_PUBLIC void CkCgi_putHeartbeatMs(HCkCgi cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkCgi_getIdleTimeoutMs(HCkCgi cHandle);
CK_VISIBLE_PUBLIC void CkCgi_putIdleTimeoutMs(HCkCgi cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkCgi_getLastErrorHtml(HCkCgi cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCgi_lastErrorHtml(HCkCgi cHandle);
CK_VISIBLE_PUBLIC void CkCgi_getLastErrorText(HCkCgi cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCgi_lastErrorText(HCkCgi cHandle);
CK_VISIBLE_PUBLIC void CkCgi_getLastErrorXml(HCkCgi cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCgi_lastErrorXml(HCkCgi cHandle);
CK_VISIBLE_PUBLIC int CkCgi_getNumParams(HCkCgi cHandle);
CK_VISIBLE_PUBLIC int CkCgi_getNumUploadFiles(HCkCgi cHandle);
CK_VISIBLE_PUBLIC int CkCgi_getReadChunkSize(HCkCgi cHandle);
CK_VISIBLE_PUBLIC void CkCgi_putReadChunkSize(HCkCgi cHandle, int newVal);
CK_VISIBLE_PUBLIC unsigned long CkCgi_getSizeLimitKB(HCkCgi cHandle);
CK_VISIBLE_PUBLIC void CkCgi_putSizeLimitKB(HCkCgi cHandle, unsigned long newVal);
CK_VISIBLE_PUBLIC BOOL CkCgi_getStreamToUploadDir(HCkCgi cHandle);
CK_VISIBLE_PUBLIC void CkCgi_putStreamToUploadDir(HCkCgi cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCgi_getUploadDir(HCkCgi cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCgi_putUploadDir(HCkCgi cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCgi_uploadDir(HCkCgi cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgi_getUtf8(HCkCgi cHandle);
CK_VISIBLE_PUBLIC void CkCgi_putUtf8(HCkCgi cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkCgi_getVerboseLogging(HCkCgi cHandle);
CK_VISIBLE_PUBLIC void CkCgi_putVerboseLogging(HCkCgi cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCgi_getVersion(HCkCgi cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCgi_version(HCkCgi cHandle);
CK_VISIBLE_PUBLIC void CkCgi_AbortAsync(HCkCgi cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgi_GetEnv(HCkCgi cHandle, const char *varName, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCgi_getEnv(HCkCgi cHandle, const char *varName);
CK_VISIBLE_PUBLIC BOOL CkCgi_GetParam(HCkCgi cHandle, const char *paramName, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCgi_getParam(HCkCgi cHandle, const char *paramName);
CK_VISIBLE_PUBLIC BOOL CkCgi_GetParamName(HCkCgi cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCgi_getParamName(HCkCgi cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCgi_GetParamValue(HCkCgi cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCgi_getParamValue(HCkCgi cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCgi_GetRawPostData(HCkCgi cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCgi_GetUploadData(HCkCgi cHandle, int index, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCgi_GetUploadFilename(HCkCgi cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCgi_getUploadFilename(HCkCgi cHandle, int index);
CK_VISIBLE_PUBLIC unsigned long CkCgi_GetUploadSize(HCkCgi cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCgi_IsGet(HCkCgi cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgi_IsHead(HCkCgi cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgi_IsPost(HCkCgi cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgi_IsUpload(HCkCgi cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgi_ReadRequest(HCkCgi cHandle);
CK_VISIBLE_PUBLIC BOOL CkCgi_SaveLastError(HCkCgi cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCgi_SaveNthToUploadDir(HCkCgi cHandle, int index);
CK_VISIBLE_PUBLIC void CkCgi_SleepMs(HCkCgi cHandle, int millisec);
CK_VISIBLE_PUBLIC BOOL CkCgi_TestConsumeAspUpload(HCkCgi cHandle, const char *path);
#endif
