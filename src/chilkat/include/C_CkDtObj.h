// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkDtObj_H
#define _C_CkDtObj_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkDtObj CkDtObj_Create(void);
CK_VISIBLE_PUBLIC void CkDtObj_Dispose(HCkDtObj handle);
CK_VISIBLE_PUBLIC int CkDtObj_getDay(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_putDay(HCkDtObj cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkDtObj_getDebugLogFilePath(HCkDtObj cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDtObj_putDebugLogFilePath(HCkDtObj cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkDtObj_debugLogFilePath(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC int CkDtObj_getHour(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_putHour(HCkDtObj cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkDtObj_getLastErrorHtml(HCkDtObj cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDtObj_lastErrorHtml(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_getLastErrorText(HCkDtObj cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDtObj_lastErrorText(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_getLastErrorXml(HCkDtObj cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDtObj_lastErrorXml(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC int CkDtObj_getMinute(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_putMinute(HCkDtObj cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkDtObj_getMonth(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_putMonth(HCkDtObj cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkDtObj_getSecond(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_putSecond(HCkDtObj cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkDtObj_getStructTmMonth(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_putStructTmMonth(HCkDtObj cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkDtObj_getStructTmYear(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_putStructTmYear(HCkDtObj cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkDtObj_getUtc(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_putUtc(HCkDtObj cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkDtObj_getUtf8(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_putUtf8(HCkDtObj cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkDtObj_getVerboseLogging(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_putVerboseLogging(HCkDtObj cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDtObj_getVersion(HCkDtObj cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkDtObj_version(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC int CkDtObj_getYear(HCkDtObj cHandle);
CK_VISIBLE_PUBLIC void CkDtObj_putYear(HCkDtObj cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkDtObj_DeSerialize(HCkDtObj cHandle, const char *serializedDtObj);
CK_VISIBLE_PUBLIC BOOL CkDtObj_SaveLastError(HCkDtObj cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkDtObj_Serialize(HCkDtObj cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkDtObj_serialize(HCkDtObj cHandle);
#endif
