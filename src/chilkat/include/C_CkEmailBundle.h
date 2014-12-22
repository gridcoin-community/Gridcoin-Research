// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkEmailBundle_H
#define _C_CkEmailBundle_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkEmailBundle CkEmailBundle_Create(void);
CK_VISIBLE_PUBLIC void CkEmailBundle_Dispose(HCkEmailBundle handle);
CK_VISIBLE_PUBLIC void CkEmailBundle_getDebugLogFilePath(HCkEmailBundle cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkEmailBundle_putDebugLogFilePath(HCkEmailBundle cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkEmailBundle_debugLogFilePath(HCkEmailBundle cHandle);
CK_VISIBLE_PUBLIC void CkEmailBundle_getLastErrorHtml(HCkEmailBundle cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkEmailBundle_lastErrorHtml(HCkEmailBundle cHandle);
CK_VISIBLE_PUBLIC void CkEmailBundle_getLastErrorText(HCkEmailBundle cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkEmailBundle_lastErrorText(HCkEmailBundle cHandle);
CK_VISIBLE_PUBLIC void CkEmailBundle_getLastErrorXml(HCkEmailBundle cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkEmailBundle_lastErrorXml(HCkEmailBundle cHandle);
CK_VISIBLE_PUBLIC int CkEmailBundle_getMessageCount(HCkEmailBundle cHandle);
CK_VISIBLE_PUBLIC BOOL CkEmailBundle_getUtf8(HCkEmailBundle cHandle);
CK_VISIBLE_PUBLIC void CkEmailBundle_putUtf8(HCkEmailBundle cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkEmailBundle_getVerboseLogging(HCkEmailBundle cHandle);
CK_VISIBLE_PUBLIC void CkEmailBundle_putVerboseLogging(HCkEmailBundle cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkEmailBundle_getVersion(HCkEmailBundle cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkEmailBundle_version(HCkEmailBundle cHandle);
CK_VISIBLE_PUBLIC BOOL CkEmailBundle_AddEmail(HCkEmailBundle cHandle, HCkEmail email);
CK_VISIBLE_PUBLIC HCkEmail CkEmailBundle_FindByHeader(HCkEmailBundle cHandle, const char *headerFieldName, const char *headerFieldValue);
CK_VISIBLE_PUBLIC HCkEmail CkEmailBundle_GetEmail(HCkEmailBundle cHandle, int index);
CK_VISIBLE_PUBLIC HCkStringArray CkEmailBundle_GetUidls(HCkEmailBundle cHandle);
CK_VISIBLE_PUBLIC BOOL CkEmailBundle_GetXml(HCkEmailBundle cHandle, HCkString outXml);
CK_VISIBLE_PUBLIC const char *CkEmailBundle_getXml(HCkEmailBundle cHandle);
CK_VISIBLE_PUBLIC BOOL CkEmailBundle_LoadXml(HCkEmailBundle cHandle, const char *filename);
CK_VISIBLE_PUBLIC BOOL CkEmailBundle_LoadXmlString(HCkEmailBundle cHandle, const char *xmlStr);
CK_VISIBLE_PUBLIC BOOL CkEmailBundle_RemoveEmail(HCkEmailBundle cHandle, HCkEmail email);
CK_VISIBLE_PUBLIC BOOL CkEmailBundle_RemoveEmailByIndex(HCkEmailBundle cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkEmailBundle_SaveLastError(HCkEmailBundle cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkEmailBundle_SaveXml(HCkEmailBundle cHandle, const char *filename);
CK_VISIBLE_PUBLIC void CkEmailBundle_SortByDate(HCkEmailBundle cHandle, BOOL ascending);
CK_VISIBLE_PUBLIC void CkEmailBundle_SortByRecipient(HCkEmailBundle cHandle, BOOL ascending);
CK_VISIBLE_PUBLIC void CkEmailBundle_SortBySender(HCkEmailBundle cHandle, BOOL ascending);
CK_VISIBLE_PUBLIC void CkEmailBundle_SortBySubject(HCkEmailBundle cHandle, BOOL ascending);
#endif
