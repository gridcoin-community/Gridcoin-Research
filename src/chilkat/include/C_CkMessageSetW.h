// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkMessageSetWH
#define _C_CkMessageSetWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkMessageSetW CkMessageSetW_Create(void);
CK_VISIBLE_PUBLIC void CkMessageSetW_Dispose(HCkMessageSetW handle);
CK_VISIBLE_PUBLIC int CkMessageSetW_getCount(HCkMessageSetW cHandle);
CK_VISIBLE_PUBLIC void CkMessageSetW_getDebugLogFilePath(HCkMessageSetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkMessageSetW_putDebugLogFilePath(HCkMessageSetW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkMessageSetW_debugLogFilePath(HCkMessageSetW cHandle);
CK_VISIBLE_PUBLIC BOOL CkMessageSetW_getHasUids(HCkMessageSetW cHandle);
CK_VISIBLE_PUBLIC void CkMessageSetW_putHasUids(HCkMessageSetW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkMessageSetW_getLastErrorHtml(HCkMessageSetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkMessageSetW_lastErrorHtml(HCkMessageSetW cHandle);
CK_VISIBLE_PUBLIC void CkMessageSetW_getLastErrorText(HCkMessageSetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkMessageSetW_lastErrorText(HCkMessageSetW cHandle);
CK_VISIBLE_PUBLIC void CkMessageSetW_getLastErrorXml(HCkMessageSetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkMessageSetW_lastErrorXml(HCkMessageSetW cHandle);
CK_VISIBLE_PUBLIC BOOL CkMessageSetW_getVerboseLogging(HCkMessageSetW cHandle);
CK_VISIBLE_PUBLIC void CkMessageSetW_putVerboseLogging(HCkMessageSetW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkMessageSetW_getVersion(HCkMessageSetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkMessageSetW_version(HCkMessageSetW cHandle);
CK_VISIBLE_PUBLIC BOOL CkMessageSetW_ContainsId(HCkMessageSetW cHandle, int msgId);
CK_VISIBLE_PUBLIC BOOL CkMessageSetW_FromCompactString(HCkMessageSetW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC int CkMessageSetW_GetId(HCkMessageSetW cHandle, int index);
CK_VISIBLE_PUBLIC void CkMessageSetW_InsertId(HCkMessageSetW cHandle, int id);
CK_VISIBLE_PUBLIC void CkMessageSetW_RemoveId(HCkMessageSetW cHandle, int id);
CK_VISIBLE_PUBLIC BOOL CkMessageSetW_SaveLastError(HCkMessageSetW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkMessageSetW_ToCommaSeparatedStr(HCkMessageSetW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkMessageSetW_toCommaSeparatedStr(HCkMessageSetW cHandle);
CK_VISIBLE_PUBLIC BOOL CkMessageSetW_ToCompactString(HCkMessageSetW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkMessageSetW_toCompactString(HCkMessageSetW cHandle);
#endif
