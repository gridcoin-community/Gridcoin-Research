// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkMessageSet_H
#define _C_CkMessageSet_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkMessageSet CkMessageSet_Create(void);
CK_VISIBLE_PUBLIC void CkMessageSet_Dispose(HCkMessageSet handle);
CK_VISIBLE_PUBLIC int CkMessageSet_getCount(HCkMessageSet cHandle);
CK_VISIBLE_PUBLIC void CkMessageSet_getDebugLogFilePath(HCkMessageSet cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkMessageSet_putDebugLogFilePath(HCkMessageSet cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkMessageSet_debugLogFilePath(HCkMessageSet cHandle);
CK_VISIBLE_PUBLIC BOOL CkMessageSet_getHasUids(HCkMessageSet cHandle);
CK_VISIBLE_PUBLIC void CkMessageSet_putHasUids(HCkMessageSet cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkMessageSet_getLastErrorHtml(HCkMessageSet cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkMessageSet_lastErrorHtml(HCkMessageSet cHandle);
CK_VISIBLE_PUBLIC void CkMessageSet_getLastErrorText(HCkMessageSet cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkMessageSet_lastErrorText(HCkMessageSet cHandle);
CK_VISIBLE_PUBLIC void CkMessageSet_getLastErrorXml(HCkMessageSet cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkMessageSet_lastErrorXml(HCkMessageSet cHandle);
CK_VISIBLE_PUBLIC BOOL CkMessageSet_getUtf8(HCkMessageSet cHandle);
CK_VISIBLE_PUBLIC void CkMessageSet_putUtf8(HCkMessageSet cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkMessageSet_getVerboseLogging(HCkMessageSet cHandle);
CK_VISIBLE_PUBLIC void CkMessageSet_putVerboseLogging(HCkMessageSet cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkMessageSet_getVersion(HCkMessageSet cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkMessageSet_version(HCkMessageSet cHandle);
CK_VISIBLE_PUBLIC BOOL CkMessageSet_ContainsId(HCkMessageSet cHandle, int msgId);
CK_VISIBLE_PUBLIC BOOL CkMessageSet_FromCompactString(HCkMessageSet cHandle, const char *str);
CK_VISIBLE_PUBLIC int CkMessageSet_GetId(HCkMessageSet cHandle, int index);
CK_VISIBLE_PUBLIC void CkMessageSet_InsertId(HCkMessageSet cHandle, int id);
CK_VISIBLE_PUBLIC void CkMessageSet_RemoveId(HCkMessageSet cHandle, int id);
CK_VISIBLE_PUBLIC BOOL CkMessageSet_SaveLastError(HCkMessageSet cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkMessageSet_ToCommaSeparatedStr(HCkMessageSet cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkMessageSet_toCommaSeparatedStr(HCkMessageSet cHandle);
CK_VISIBLE_PUBLIC BOOL CkMessageSet_ToCompactString(HCkMessageSet cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkMessageSet_toCompactString(HCkMessageSet cHandle);
#endif
