// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkHttpResponseWH
#define _C_CkHttpResponseWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkHttpResponseW CkHttpResponseW_Create(void);
CK_VISIBLE_PUBLIC void CkHttpResponseW_Dispose(HCkHttpResponseW handle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getBody(HCkHttpResponseW cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getBodyQP(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_bodyQP(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getBodyStr(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_bodyStr(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getCharset(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_charset(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC unsigned long CkHttpResponseW_getContentLength(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC __int64 CkHttpResponseW_getContentLength64(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getDate(HCkHttpResponseW cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getDateStr(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_dateStr(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getDebugLogFilePath(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpResponseW_putDebugLogFilePath(HCkHttpResponseW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_debugLogFilePath(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getDomain(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_domain(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getFullMime(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_fullMime(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getHeader(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_header(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getLastErrorHtml(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_lastErrorHtml(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getLastErrorText(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_lastErrorText(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getLastErrorXml(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_lastErrorXml(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC int CkHttpResponseW_getNumCookies(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC int CkHttpResponseW_getNumHeaderFields(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC int CkHttpResponseW_getStatusCode(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getStatusLine(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_statusLine(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_getVerboseLogging(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponseW_putVerboseLogging(HCkHttpResponseW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHttpResponseW_getVersion(HCkHttpResponseW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_version(HCkHttpResponseW cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_GetCookieDomain(HCkHttpResponseW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_getCookieDomain(HCkHttpResponseW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_GetCookieExpires(HCkHttpResponseW cHandle, int index, SYSTEMTIME *outSysTime);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_GetCookieExpiresStr(HCkHttpResponseW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_getCookieExpiresStr(HCkHttpResponseW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_GetCookieName(HCkHttpResponseW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_getCookieName(HCkHttpResponseW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_GetCookiePath(HCkHttpResponseW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_getCookiePath(HCkHttpResponseW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_GetCookieValue(HCkHttpResponseW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_getCookieValue(HCkHttpResponseW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_GetHeaderField(HCkHttpResponseW cHandle, const wchar_t *fieldName, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_getHeaderField(HCkHttpResponseW cHandle, const wchar_t *fieldName);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_GetHeaderFieldAttr(HCkHttpResponseW cHandle, const wchar_t *fieldName, const wchar_t *attrName, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_getHeaderFieldAttr(HCkHttpResponseW cHandle, const wchar_t *fieldName, const wchar_t *attrName);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_GetHeaderName(HCkHttpResponseW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_getHeaderName(HCkHttpResponseW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_GetHeaderValue(HCkHttpResponseW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_getHeaderValue(HCkHttpResponseW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_SaveBodyBinary(HCkHttpResponseW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_SaveBodyText(HCkHttpResponseW cHandle, BOOL bCrlf, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_SaveLastError(HCkHttpResponseW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkHttpResponseW_UrlEncParamValue(HCkHttpResponseW cHandle, const wchar_t *encodedParamString, const wchar_t *paramName, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkHttpResponseW_urlEncParamValue(HCkHttpResponseW cHandle, const wchar_t *encodedParamString, const wchar_t *paramName);
#endif
