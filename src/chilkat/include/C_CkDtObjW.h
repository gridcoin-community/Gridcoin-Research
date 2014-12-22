// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkDtObjWH
#define _C_CkDtObjWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkDtObjW CkDtObjW_Create(void);
CK_VISIBLE_PUBLIC void CkDtObjW_Dispose(HCkDtObjW handle);
CK_VISIBLE_PUBLIC int CkDtObjW_getDay(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_putDay(HCkDtObjW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkDtObjW_getDebugLogFilePath(HCkDtObjW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDtObjW_putDebugLogFilePath(HCkDtObjW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkDtObjW_debugLogFilePath(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC int CkDtObjW_getHour(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_putHour(HCkDtObjW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkDtObjW_getLastErrorHtml(HCkDtObjW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDtObjW_lastErrorHtml(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_getLastErrorText(HCkDtObjW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDtObjW_lastErrorText(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_getLastErrorXml(HCkDtObjW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDtObjW_lastErrorXml(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC int CkDtObjW_getMinute(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_putMinute(HCkDtObjW cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkDtObjW_getMonth(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_putMonth(HCkDtObjW cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkDtObjW_getSecond(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_putSecond(HCkDtObjW cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkDtObjW_getStructTmMonth(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_putStructTmMonth(HCkDtObjW cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkDtObjW_getStructTmYear(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_putStructTmYear(HCkDtObjW cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkDtObjW_getUtc(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_putUtc(HCkDtObjW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkDtObjW_getVerboseLogging(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_putVerboseLogging(HCkDtObjW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDtObjW_getVersion(HCkDtObjW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDtObjW_version(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC int CkDtObjW_getYear(HCkDtObjW cHandle);
CK_VISIBLE_PUBLIC void CkDtObjW_putYear(HCkDtObjW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkDtObjW_DeSerialize(HCkDtObjW cHandle, const wchar_t *serializedDtObj);
CK_VISIBLE_PUBLIC BOOL CkDtObjW_SaveLastError(HCkDtObjW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDtObjW_Serialize(HCkDtObjW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDtObjW_serialize(HCkDtObjW cHandle);
#endif
