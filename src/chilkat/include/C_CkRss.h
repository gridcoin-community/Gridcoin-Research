// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkRss_H
#define _C_CkRss_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkRss CkRss_Create(void);
CK_VISIBLE_PUBLIC void CkRss_Dispose(HCkRss handle);
CK_VISIBLE_PUBLIC void CkRss_getDebugLogFilePath(HCkRss cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkRss_putDebugLogFilePath(HCkRss cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkRss_debugLogFilePath(HCkRss cHandle);
CK_VISIBLE_PUBLIC void CkRss_getLastErrorHtml(HCkRss cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkRss_lastErrorHtml(HCkRss cHandle);
CK_VISIBLE_PUBLIC void CkRss_getLastErrorText(HCkRss cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkRss_lastErrorText(HCkRss cHandle);
CK_VISIBLE_PUBLIC void CkRss_getLastErrorXml(HCkRss cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkRss_lastErrorXml(HCkRss cHandle);
CK_VISIBLE_PUBLIC int CkRss_getNumChannels(HCkRss cHandle);
CK_VISIBLE_PUBLIC int CkRss_getNumItems(HCkRss cHandle);
CK_VISIBLE_PUBLIC BOOL CkRss_getUtf8(HCkRss cHandle);
CK_VISIBLE_PUBLIC void CkRss_putUtf8(HCkRss cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkRss_getVerboseLogging(HCkRss cHandle);
CK_VISIBLE_PUBLIC void CkRss_putVerboseLogging(HCkRss cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkRss_getVersion(HCkRss cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkRss_version(HCkRss cHandle);
CK_VISIBLE_PUBLIC HCkRss CkRss_AddNewChannel(HCkRss cHandle);
CK_VISIBLE_PUBLIC HCkRss CkRss_AddNewImage(HCkRss cHandle);
CK_VISIBLE_PUBLIC HCkRss CkRss_AddNewItem(HCkRss cHandle);
CK_VISIBLE_PUBLIC BOOL CkRss_DownloadRss(HCkRss cHandle, const char *url);
CK_VISIBLE_PUBLIC BOOL CkRss_GetAttr(HCkRss cHandle, const char *tag, const char *attrName, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkRss_getAttr(HCkRss cHandle, const char *tag, const char *attrName);
CK_VISIBLE_PUBLIC HCkRss CkRss_GetChannel(HCkRss cHandle, int index);
CK_VISIBLE_PUBLIC int CkRss_GetCount(HCkRss cHandle, const char *tag);
CK_VISIBLE_PUBLIC BOOL CkRss_GetDate(HCkRss cHandle, const char *tag, SYSTEMTIME *outSysTime);
CK_VISIBLE_PUBLIC BOOL CkRss_GetDateStr(HCkRss cHandle, const char *tag, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkRss_getDateStr(HCkRss cHandle, const char *tag);
CK_VISIBLE_PUBLIC HCkRss CkRss_GetImage(HCkRss cHandle);
CK_VISIBLE_PUBLIC int CkRss_GetInt(HCkRss cHandle, const char *tag);
CK_VISIBLE_PUBLIC HCkRss CkRss_GetItem(HCkRss cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkRss_GetString(HCkRss cHandle, const char *tag, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkRss_getString(HCkRss cHandle, const char *tag);
CK_VISIBLE_PUBLIC BOOL CkRss_LoadRssFile(HCkRss cHandle, const char *filePath);
CK_VISIBLE_PUBLIC BOOL CkRss_LoadRssString(HCkRss cHandle, const char *rssString);
CK_VISIBLE_PUBLIC BOOL CkRss_MGetAttr(HCkRss cHandle, const char *tag, int index, const char *attrName, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkRss_mGetAttr(HCkRss cHandle, const char *tag, int index, const char *attrName);
CK_VISIBLE_PUBLIC BOOL CkRss_MGetString(HCkRss cHandle, const char *tag, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkRss_mGetString(HCkRss cHandle, const char *tag, int index);
CK_VISIBLE_PUBLIC BOOL CkRss_MSetAttr(HCkRss cHandle, const char *tag, int idx, const char *attrName, const char *value);
CK_VISIBLE_PUBLIC BOOL CkRss_MSetString(HCkRss cHandle, const char *tag, int idx, const char *value);
CK_VISIBLE_PUBLIC void CkRss_NewRss(HCkRss cHandle);
CK_VISIBLE_PUBLIC void CkRss_Remove(HCkRss cHandle, const char *tag);
CK_VISIBLE_PUBLIC BOOL CkRss_SaveLastError(HCkRss cHandle, const char *path);
CK_VISIBLE_PUBLIC void CkRss_SetAttr(HCkRss cHandle, const char *tag, const char *attrName, const char *value);
CK_VISIBLE_PUBLIC void CkRss_SetDate(HCkRss cHandle, const char *tag, SYSTEMTIME *dateTime);
CK_VISIBLE_PUBLIC void CkRss_SetDateNow(HCkRss cHandle, const char *tag);
CK_VISIBLE_PUBLIC void CkRss_SetDateStr(HCkRss cHandle, const char *tag, const char *dateTimeStr);
CK_VISIBLE_PUBLIC void CkRss_SetInt(HCkRss cHandle, const char *tag, int value);
CK_VISIBLE_PUBLIC void CkRss_SetString(HCkRss cHandle, const char *tag, const char *value);
CK_VISIBLE_PUBLIC BOOL CkRss_ToXmlString(HCkRss cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkRss_toXmlString(HCkRss cHandle);
#endif
