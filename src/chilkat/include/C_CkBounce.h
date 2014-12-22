// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkBounce_H
#define _C_CkBounce_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkBounce CkBounce_Create(void);
CK_VISIBLE_PUBLIC void CkBounce_Dispose(HCkBounce handle);
CK_VISIBLE_PUBLIC void CkBounce_getBounceAddress(HCkBounce cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkBounce_bounceAddress(HCkBounce cHandle);
CK_VISIBLE_PUBLIC void CkBounce_getBounceData(HCkBounce cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkBounce_bounceData(HCkBounce cHandle);
CK_VISIBLE_PUBLIC int CkBounce_getBounceType(HCkBounce cHandle);
CK_VISIBLE_PUBLIC void CkBounce_getDebugLogFilePath(HCkBounce cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkBounce_putDebugLogFilePath(HCkBounce cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkBounce_debugLogFilePath(HCkBounce cHandle);
CK_VISIBLE_PUBLIC void CkBounce_getLastErrorHtml(HCkBounce cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkBounce_lastErrorHtml(HCkBounce cHandle);
CK_VISIBLE_PUBLIC void CkBounce_getLastErrorText(HCkBounce cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkBounce_lastErrorText(HCkBounce cHandle);
CK_VISIBLE_PUBLIC void CkBounce_getLastErrorXml(HCkBounce cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkBounce_lastErrorXml(HCkBounce cHandle);
CK_VISIBLE_PUBLIC BOOL CkBounce_getUtf8(HCkBounce cHandle);
CK_VISIBLE_PUBLIC void CkBounce_putUtf8(HCkBounce cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkBounce_getVerboseLogging(HCkBounce cHandle);
CK_VISIBLE_PUBLIC void CkBounce_putVerboseLogging(HCkBounce cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkBounce_getVersion(HCkBounce cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkBounce_version(HCkBounce cHandle);
CK_VISIBLE_PUBLIC BOOL CkBounce_ExamineEmail(HCkBounce cHandle, HCkEmail email);
CK_VISIBLE_PUBLIC BOOL CkBounce_ExamineEml(HCkBounce cHandle, const char *emlFilename);
CK_VISIBLE_PUBLIC BOOL CkBounce_ExamineMime(HCkBounce cHandle, const char *mimeText);
CK_VISIBLE_PUBLIC BOOL CkBounce_SaveLastError(HCkBounce cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkBounce_UnlockComponent(HCkBounce cHandle, const char *unlockCode);
#endif
