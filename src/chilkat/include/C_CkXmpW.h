// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkXmpWH
#define _C_CkXmpWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkXmpW CkXmpW_Create(void);
CK_VISIBLE_PUBLIC void CkXmpW_Dispose(HCkXmpW handle);
CK_VISIBLE_PUBLIC void CkXmpW_getDebugLogFilePath(HCkXmpW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkXmpW_putDebugLogFilePath(HCkXmpW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkXmpW_debugLogFilePath(HCkXmpW cHandle);
CK_VISIBLE_PUBLIC void CkXmpW_getLastErrorHtml(HCkXmpW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkXmpW_lastErrorHtml(HCkXmpW cHandle);
CK_VISIBLE_PUBLIC void CkXmpW_getLastErrorText(HCkXmpW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkXmpW_lastErrorText(HCkXmpW cHandle);
CK_VISIBLE_PUBLIC void CkXmpW_getLastErrorXml(HCkXmpW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkXmpW_lastErrorXml(HCkXmpW cHandle);
CK_VISIBLE_PUBLIC int CkXmpW_getNumEmbedded(HCkXmpW cHandle);
CK_VISIBLE_PUBLIC BOOL CkXmpW_getStructInnerDescrip(HCkXmpW cHandle);
CK_VISIBLE_PUBLIC void CkXmpW_putStructInnerDescrip(HCkXmpW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkXmpW_getVerboseLogging(HCkXmpW cHandle);
CK_VISIBLE_PUBLIC void CkXmpW_putVerboseLogging(HCkXmpW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkXmpW_getVersion(HCkXmpW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkXmpW_version(HCkXmpW cHandle);
CK_VISIBLE_PUBLIC BOOL CkXmpW_AddArray(HCkXmpW cHandle, HCkXmlW xml, const wchar_t *arrType, const wchar_t *propName, HCkStringArrayW values);
CK_VISIBLE_PUBLIC void CkXmpW_AddNsMapping(HCkXmpW cHandle, const wchar_t *ns, const wchar_t *uri);
CK_VISIBLE_PUBLIC BOOL CkXmpW_AddSimpleDate(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *propName, SYSTEMTIME *propVal);
CK_VISIBLE_PUBLIC BOOL CkXmpW_AddSimpleInt(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *propName, int propVal);
CK_VISIBLE_PUBLIC BOOL CkXmpW_AddSimpleStr(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *propName, const wchar_t *propVal);
CK_VISIBLE_PUBLIC BOOL CkXmpW_AddStructProp(HCkXmpW cHandle, HCkXmlW iChilkatXml, const wchar_t *structName, const wchar_t *propName, const wchar_t *propValue);
CK_VISIBLE_PUBLIC BOOL CkXmpW_Append(HCkXmpW cHandle, HCkXmlW iXml);
CK_VISIBLE_PUBLIC BOOL CkXmpW_DateToString(HCkXmpW cHandle, SYSTEMTIME *d, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkXmpW_dateToString(HCkXmpW cHandle, SYSTEMTIME *d);
CK_VISIBLE_PUBLIC HCkStringArrayW CkXmpW_GetArray(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *propName);
CK_VISIBLE_PUBLIC HCkXmlW CkXmpW_GetEmbedded(HCkXmpW cHandle, int index);
CK_VISIBLE_PUBLIC HCkXmlW CkXmpW_GetProperty(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *propName);
CK_VISIBLE_PUBLIC BOOL CkXmpW_GetSimpleDate(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *propName, SYSTEMTIME *outSysTime);
CK_VISIBLE_PUBLIC int CkXmpW_GetSimpleInt(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *propName);
CK_VISIBLE_PUBLIC BOOL CkXmpW_GetSimpleStr(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *propName, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkXmpW_getSimpleStr(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *propName);
CK_VISIBLE_PUBLIC HCkStringArrayW CkXmpW_GetStructPropNames(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *structName);
CK_VISIBLE_PUBLIC BOOL CkXmpW_GetStructValue(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *structName, const wchar_t *propName, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkXmpW_getStructValue(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *structName, const wchar_t *propName);
CK_VISIBLE_PUBLIC BOOL CkXmpW_LoadAppFile(HCkXmpW cHandle, const wchar_t *filename);
CK_VISIBLE_PUBLIC BOOL CkXmpW_LoadFromBuffer(HCkXmpW cHandle, HCkByteData fileData, const wchar_t *ext);
CK_VISIBLE_PUBLIC HCkXmlW CkXmpW_NewXmp(HCkXmpW cHandle);
CK_VISIBLE_PUBLIC BOOL CkXmpW_RemoveAllEmbedded(HCkXmpW cHandle);
CK_VISIBLE_PUBLIC BOOL CkXmpW_RemoveArray(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *propName);
CK_VISIBLE_PUBLIC BOOL CkXmpW_RemoveEmbedded(HCkXmpW cHandle, int index);
CK_VISIBLE_PUBLIC void CkXmpW_RemoveNsMapping(HCkXmpW cHandle, const wchar_t *ns);
CK_VISIBLE_PUBLIC BOOL CkXmpW_RemoveSimple(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *propName);
CK_VISIBLE_PUBLIC BOOL CkXmpW_RemoveStruct(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *structName);
CK_VISIBLE_PUBLIC BOOL CkXmpW_RemoveStructProp(HCkXmpW cHandle, HCkXmlW iXml, const wchar_t *structName, const wchar_t *propName);
CK_VISIBLE_PUBLIC BOOL CkXmpW_SaveAppFile(HCkXmpW cHandle, const wchar_t *filename);
CK_VISIBLE_PUBLIC BOOL CkXmpW_SaveLastError(HCkXmpW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkXmpW_SaveToBuffer(HCkXmpW cHandle, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkXmpW_StringToDate(HCkXmpW cHandle, const wchar_t *str, SYSTEMTIME *outSysTime);
CK_VISIBLE_PUBLIC BOOL CkXmpW_UnlockComponent(HCkXmpW cHandle, const wchar_t *unlockCode);
#endif
