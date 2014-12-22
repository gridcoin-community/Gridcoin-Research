// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkDateTimeWH
#define _C_CkDateTimeWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkDateTimeW CkDateTimeW_Create(void);
CK_VISIBLE_PUBLIC void CkDateTimeW_Dispose(HCkDateTimeW handle);
CK_VISIBLE_PUBLIC void CkDateTimeW_getDebugLogFilePath(HCkDateTimeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkDateTimeW_putDebugLogFilePath(HCkDateTimeW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkDateTimeW_debugLogFilePath(HCkDateTimeW cHandle);
CK_VISIBLE_PUBLIC int CkDateTimeW_getIsDst(HCkDateTimeW cHandle);
CK_VISIBLE_PUBLIC void CkDateTimeW_getLastErrorHtml(HCkDateTimeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDateTimeW_lastErrorHtml(HCkDateTimeW cHandle);
CK_VISIBLE_PUBLIC void CkDateTimeW_getLastErrorText(HCkDateTimeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDateTimeW_lastErrorText(HCkDateTimeW cHandle);
CK_VISIBLE_PUBLIC void CkDateTimeW_getLastErrorXml(HCkDateTimeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDateTimeW_lastErrorXml(HCkDateTimeW cHandle);
CK_VISIBLE_PUBLIC int CkDateTimeW_getUtcOffset(HCkDateTimeW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDateTimeW_getVerboseLogging(HCkDateTimeW cHandle);
CK_VISIBLE_PUBLIC void CkDateTimeW_putVerboseLogging(HCkDateTimeW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkDateTimeW_getVersion(HCkDateTimeW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkDateTimeW_version(HCkDateTimeW cHandle);
CK_VISIBLE_PUBLIC BOOL CkDateTimeW_AddDays(HCkDateTimeW cHandle, int numDays);
CK_VISIBLE_PUBLIC void CkDateTimeW_DeSerialize(HCkDateTimeW cHandle, const wchar_t *serializedDateTime);
CK_VISIBLE_PUBLIC __int64 CkDateTimeW_GetAsDateTimeTicks(HCkDateTimeW cHandle, BOOL bLocal);
#if !defined(CK_USE_UINT_T)
CK_VISIBLE_PUBLIC unsigned long CkDateTimeW_GetAsDosDate(HCkDateTimeW cHandle, BOOL bLocal);
#endif
CK_VISIBLE_PUBLIC double CkDateTimeW_GetAsOleDate(HCkDateTimeW cHandle, BOOL bLocal);
CK_VISIBLE_PUBLIC BOOL CkDateTimeW_GetAsRfc822(HCkDateTimeW cHandle, BOOL bLocal, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDateTimeW_getAsRfc822(HCkDateTimeW cHandle, BOOL bLocal);
CK_VISIBLE_PUBLIC void CkDateTimeW_GetAsSystemTime(HCkDateTimeW cHandle, BOOL bLocal, SYSTEMTIME *outSysTime);
CK_VISIBLE_PUBLIC void CkDateTimeW_GetAsTmStruct(HCkDateTimeW cHandle, BOOL bLocal, struct tm *tmbuf);
CK_VISIBLE_PUBLIC time_t CkDateTimeW_GetAsUnixTime(HCkDateTimeW cHandle, BOOL bLocal);
CK_VISIBLE_PUBLIC __int64 CkDateTimeW_GetAsUnixTime64(HCkDateTimeW cHandle, BOOL bLocal);
CK_VISIBLE_PUBLIC double CkDateTimeW_GetAsUnixTimeDbl(HCkDateTimeW cHandle, BOOL bLocal);
#if !defined(CK_USE_UINT_T)
CK_VISIBLE_PUBLIC unsigned short CkDateTimeW_GetDosDateHigh(HCkDateTimeW cHandle, BOOL bLocal);
#endif
#if !defined(CK_USE_UINT_T)
CK_VISIBLE_PUBLIC unsigned short CkDateTimeW_GetDosDateLow(HCkDateTimeW cHandle, BOOL bLocal);
#endif
CK_VISIBLE_PUBLIC BOOL CkDateTimeW_SaveLastError(HCkDateTimeW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkDateTimeW_Serialize(HCkDateTimeW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkDateTimeW_serialize(HCkDateTimeW cHandle);
CK_VISIBLE_PUBLIC void CkDateTimeW_SetFromCurrentSystemTime(HCkDateTimeW cHandle);
CK_VISIBLE_PUBLIC void CkDateTimeW_SetFromDateTimeTicks(HCkDateTimeW cHandle, BOOL bLocal, __int64 ticks);
#if !defined(CK_USE_UINT_T)
CK_VISIBLE_PUBLIC void CkDateTimeW_SetFromDosDate(HCkDateTimeW cHandle, BOOL bLocal, unsigned long t);
#endif
#if !defined(CK_USE_UINT_T)
CK_VISIBLE_PUBLIC void CkDateTimeW_SetFromDosDate2(HCkDateTimeW cHandle, BOOL bLocal, unsigned short datePart, unsigned short timePart);
#endif
CK_VISIBLE_PUBLIC void CkDateTimeW_SetFromOleDate(HCkDateTimeW cHandle, BOOL bLocal, double dt);
CK_VISIBLE_PUBLIC BOOL CkDateTimeW_SetFromRfc822(HCkDateTimeW cHandle, const wchar_t *rfc822Str);
CK_VISIBLE_PUBLIC void CkDateTimeW_SetFromSystemTime(HCkDateTimeW cHandle, BOOL bLocal, SYSTEMTIME *sysTime);
CK_VISIBLE_PUBLIC void CkDateTimeW_SetFromTmStruct(HCkDateTimeW cHandle, BOOL bLocal, struct tm *tmbuf);
CK_VISIBLE_PUBLIC void CkDateTimeW_SetFromUnixTime(HCkDateTimeW cHandle, BOOL bLocal, time_t t);
CK_VISIBLE_PUBLIC void CkDateTimeW_SetFromUnixTime64(HCkDateTimeW cHandle, BOOL bLocal, __int64 t);
CK_VISIBLE_PUBLIC void CkDateTimeW_SetFromUnixTimeDbl(HCkDateTimeW cHandle, BOOL bLocal, double d);
#endif
