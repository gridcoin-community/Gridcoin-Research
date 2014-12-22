// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkStringArrayWH
#define _C_CkStringArrayWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkStringArrayW CkStringArrayW_Create(void);
CK_VISIBLE_PUBLIC void CkStringArrayW_Dispose(HCkStringArrayW handle);
CK_VISIBLE_PUBLIC int CkStringArrayW_getCount(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_getCrlf(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC void CkStringArrayW_putCrlf(HCkStringArrayW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkStringArrayW_getDebugLogFilePath(HCkStringArrayW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkStringArrayW_putDebugLogFilePath(HCkStringArrayW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkStringArrayW_debugLogFilePath(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC void CkStringArrayW_getLastErrorHtml(HCkStringArrayW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkStringArrayW_lastErrorHtml(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC void CkStringArrayW_getLastErrorText(HCkStringArrayW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkStringArrayW_lastErrorText(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC void CkStringArrayW_getLastErrorXml(HCkStringArrayW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkStringArrayW_lastErrorXml(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC int CkStringArrayW_getLength(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_getTrim(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC void CkStringArrayW_putTrim(HCkStringArrayW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_getUnique(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC void CkStringArrayW_putUnique(HCkStringArrayW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_getVerboseLogging(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC void CkStringArrayW_putVerboseLogging(HCkStringArrayW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkStringArrayW_getVersion(HCkStringArrayW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkStringArrayW_version(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_Append(HCkStringArrayW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_AppendSerialized(HCkStringArrayW cHandle, const wchar_t *encodedStr);
CK_VISIBLE_PUBLIC void CkStringArrayW_Clear(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_Contains(HCkStringArrayW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC int CkStringArrayW_Find(HCkStringArrayW cHandle, const wchar_t *findStr, int startIndex);
CK_VISIBLE_PUBLIC int CkStringArrayW_FindFirstMatch(HCkStringArrayW cHandle, const wchar_t *matchPattern, int startIndex);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_GetString(HCkStringArrayW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkStringArrayW_getString(HCkStringArrayW cHandle, int index);
CK_VISIBLE_PUBLIC int CkStringArrayW_GetStringLen(HCkStringArrayW cHandle, int index);
CK_VISIBLE_PUBLIC void CkStringArrayW_InsertAt(HCkStringArrayW cHandle, int index, const wchar_t *str);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_LastString(HCkStringArrayW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkStringArrayW_lastString(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_LoadFromFile(HCkStringArrayW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_LoadFromFile2(HCkStringArrayW cHandle, const wchar_t *path, const wchar_t *charset);
CK_VISIBLE_PUBLIC void CkStringArrayW_LoadFromText(HCkStringArrayW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_Pop(HCkStringArrayW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkStringArrayW_pop(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC void CkStringArrayW_Prepend(HCkStringArrayW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC void CkStringArrayW_Remove(HCkStringArrayW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_RemoveAt(HCkStringArrayW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_SaveLastError(HCkStringArrayW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_SaveNthToFile(HCkStringArrayW cHandle, int index, const wchar_t *saveToPath);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_SaveToFile(HCkStringArrayW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_SaveToFile2(HCkStringArrayW cHandle, const wchar_t *saveToPath, const wchar_t *charset);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_SaveToText(HCkStringArrayW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkStringArrayW_saveToText(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_Serialize(HCkStringArrayW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkStringArrayW_serialize(HCkStringArrayW cHandle);
CK_VISIBLE_PUBLIC void CkStringArrayW_Sort(HCkStringArrayW cHandle, BOOL ascending);
CK_VISIBLE_PUBLIC void CkStringArrayW_SplitAndAppend(HCkStringArrayW cHandle, const wchar_t *str, const wchar_t *boundary);
CK_VISIBLE_PUBLIC BOOL CkStringArrayW_StrAt(HCkStringArrayW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkStringArrayW_strAt(HCkStringArrayW cHandle, int index);
CK_VISIBLE_PUBLIC void CkStringArrayW_Subtract(HCkStringArrayW cHandle, HCkStringArrayW stringArrayObj);
CK_VISIBLE_PUBLIC void CkStringArrayW_Union(HCkStringArrayW cHandle, HCkStringArrayW stringArrayObj);
#endif
