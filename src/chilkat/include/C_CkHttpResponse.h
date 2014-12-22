// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkHttpResponse_H
#define _C_CkHttpResponse_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkHttpResponse CkHttpResponse_Create(void);
CK_VISIBLE_PUBLIC void CkHttpResponse_Dispose(HCkHttpResponse handle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getBody(HCkHttpResponse cHandle, HCkByteData retval);
CK_VISIBLE_PUBLIC void CkHttpResponse_getBodyQP(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_bodyQP(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getBodyStr(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_bodyStr(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getCharset(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_charset(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC unsigned long CkHttpResponse_getContentLength(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC __int64 CkHttpResponse_getContentLength64(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getDate(HCkHttpResponse cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkHttpResponse_getDateStr(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_dateStr(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getDebugLogFilePath(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkHttpResponse_putDebugLogFilePath(HCkHttpResponse cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_debugLogFilePath(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getDomain(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_domain(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getFullMime(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_fullMime(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getHeader(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_header(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getLastErrorHtml(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_lastErrorHtml(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getLastErrorText(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_lastErrorText(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getLastErrorXml(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_lastErrorXml(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC int CkHttpResponse_getNumCookies(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC int CkHttpResponse_getNumHeaderFields(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC int CkHttpResponse_getStatusCode(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_getStatusLine(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_statusLine(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_getUtf8(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_putUtf8(HCkHttpResponse cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_getVerboseLogging(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC void CkHttpResponse_putVerboseLogging(HCkHttpResponse cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkHttpResponse_getVersion(HCkHttpResponse cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_version(HCkHttpResponse cHandle);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_GetCookieDomain(HCkHttpResponse cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_getCookieDomain(HCkHttpResponse cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_GetCookieExpires(HCkHttpResponse cHandle, int index, SYSTEMTIME *outSysTime);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_GetCookieExpiresStr(HCkHttpResponse cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_getCookieExpiresStr(HCkHttpResponse cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_GetCookieName(HCkHttpResponse cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_getCookieName(HCkHttpResponse cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_GetCookiePath(HCkHttpResponse cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_getCookiePath(HCkHttpResponse cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_GetCookieValue(HCkHttpResponse cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_getCookieValue(HCkHttpResponse cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_GetHeaderField(HCkHttpResponse cHandle, const char *fieldName, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_getHeaderField(HCkHttpResponse cHandle, const char *fieldName);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_GetHeaderFieldAttr(HCkHttpResponse cHandle, const char *fieldName, const char *attrName, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_getHeaderFieldAttr(HCkHttpResponse cHandle, const char *fieldName, const char *attrName);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_GetHeaderName(HCkHttpResponse cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_getHeaderName(HCkHttpResponse cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_GetHeaderValue(HCkHttpResponse cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_getHeaderValue(HCkHttpResponse cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_SaveBodyBinary(HCkHttpResponse cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_SaveBodyText(HCkHttpResponse cHandle, BOOL bCrlf, const char *path);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_SaveLastError(HCkHttpResponse cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkHttpResponse_UrlEncParamValue(HCkHttpResponse cHandle, const char *encodedParamString, const char *paramName, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkHttpResponse_urlEncParamValue(HCkHttpResponse cHandle, const char *encodedParamString, const char *paramName);
#endif
