// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkStringArray_H
#define _C_CkStringArray_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkStringArray CkStringArray_Create(void);
CK_VISIBLE_PUBLIC void CkStringArray_Dispose(HCkStringArray handle);
CK_VISIBLE_PUBLIC int CkStringArray_getCount(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArray_getCrlf(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC void CkStringArray_putCrlf(HCkStringArray cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkStringArray_getDebugLogFilePath(HCkStringArray cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkStringArray_putDebugLogFilePath(HCkStringArray cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkStringArray_debugLogFilePath(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC void CkStringArray_getLastErrorHtml(HCkStringArray cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkStringArray_lastErrorHtml(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC void CkStringArray_getLastErrorText(HCkStringArray cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkStringArray_lastErrorText(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC void CkStringArray_getLastErrorXml(HCkStringArray cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkStringArray_lastErrorXml(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC int CkStringArray_getLength(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArray_getTrim(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC void CkStringArray_putTrim(HCkStringArray cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkStringArray_getUnique(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC void CkStringArray_putUnique(HCkStringArray cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkStringArray_getUtf8(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC void CkStringArray_putUtf8(HCkStringArray cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkStringArray_getVerboseLogging(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC void CkStringArray_putVerboseLogging(HCkStringArray cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkStringArray_getVersion(HCkStringArray cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkStringArray_version(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArray_Append(HCkStringArray cHandle, const char *str);
CK_VISIBLE_PUBLIC BOOL CkStringArray_AppendSerialized(HCkStringArray cHandle, const char *encodedStr);
CK_VISIBLE_PUBLIC void CkStringArray_Clear(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArray_Contains(HCkStringArray cHandle, const char *str);
CK_VISIBLE_PUBLIC int CkStringArray_Find(HCkStringArray cHandle, const char *findStr, int startIndex);
CK_VISIBLE_PUBLIC int CkStringArray_FindFirstMatch(HCkStringArray cHandle, const char *matchPattern, int startIndex);
CK_VISIBLE_PUBLIC BOOL CkStringArray_GetString(HCkStringArray cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkStringArray_getString(HCkStringArray cHandle, int index);
CK_VISIBLE_PUBLIC int CkStringArray_GetStringLen(HCkStringArray cHandle, int index);
CK_VISIBLE_PUBLIC void CkStringArray_InsertAt(HCkStringArray cHandle, int index, const char *str);
CK_VISIBLE_PUBLIC BOOL CkStringArray_LastString(HCkStringArray cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkStringArray_lastString(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArray_LoadFromFile(HCkStringArray cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkStringArray_LoadFromFile2(HCkStringArray cHandle, const char *path, const char *charset);
CK_VISIBLE_PUBLIC void CkStringArray_LoadFromText(HCkStringArray cHandle, const char *str);
CK_VISIBLE_PUBLIC BOOL CkStringArray_Pop(HCkStringArray cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkStringArray_pop(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC void CkStringArray_Prepend(HCkStringArray cHandle, const char *str);
CK_VISIBLE_PUBLIC void CkStringArray_Remove(HCkStringArray cHandle, const char *str);
CK_VISIBLE_PUBLIC BOOL CkStringArray_RemoveAt(HCkStringArray cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkStringArray_SaveLastError(HCkStringArray cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkStringArray_SaveNthToFile(HCkStringArray cHandle, int index, const char *saveToPath);
CK_VISIBLE_PUBLIC BOOL CkStringArray_SaveToFile(HCkStringArray cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkStringArray_SaveToFile2(HCkStringArray cHandle, const char *saveToPath, const char *charset);
CK_VISIBLE_PUBLIC BOOL CkStringArray_SaveToText(HCkStringArray cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkStringArray_saveToText(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC BOOL CkStringArray_Serialize(HCkStringArray cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkStringArray_serialize(HCkStringArray cHandle);
CK_VISIBLE_PUBLIC void CkStringArray_Sort(HCkStringArray cHandle, BOOL ascending);
CK_VISIBLE_PUBLIC void CkStringArray_SplitAndAppend(HCkStringArray cHandle, const char *str, const char *boundary);
CK_VISIBLE_PUBLIC BOOL CkStringArray_StrAt(HCkStringArray cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkStringArray_strAt(HCkStringArray cHandle, int index);
CK_VISIBLE_PUBLIC void CkStringArray_Subtract(HCkStringArray cHandle, HCkStringArray stringArrayObj);
CK_VISIBLE_PUBLIC void CkStringArray_Union(HCkStringArray cHandle, HCkStringArray stringArrayObj);
#endif
