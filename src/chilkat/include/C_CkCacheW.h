// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCacheWH
#define _C_CkCacheWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCacheW CkCacheW_Create(void);
CK_VISIBLE_PUBLIC void CkCacheW_Dispose(HCkCacheW handle);
CK_VISIBLE_PUBLIC void CkCacheW_getDebugLogFilePath(HCkCacheW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCacheW_putDebugLogFilePath(HCkCacheW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_debugLogFilePath(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC void CkCacheW_getLastErrorHtml(HCkCacheW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_lastErrorHtml(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC void CkCacheW_getLastErrorText(HCkCacheW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_lastErrorText(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC void CkCacheW_getLastErrorXml(HCkCacheW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_lastErrorXml(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC void CkCacheW_getLastEtagFetched(HCkCacheW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_lastEtagFetched(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC void CkCacheW_getLastExpirationFetched(HCkCacheW cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkCacheW_getLastExpirationFetchedStr(HCkCacheW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_lastExpirationFetchedStr(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCacheW_getLastHitExpired(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC void CkCacheW_getLastKeyFetched(HCkCacheW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_lastKeyFetched(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC int CkCacheW_getLevel(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC void CkCacheW_putLevel(HCkCacheW cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkCacheW_getNumRoots(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCacheW_getVerboseLogging(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC void CkCacheW_putVerboseLogging(HCkCacheW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCacheW_getVersion(HCkCacheW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_version(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC void CkCacheW_AddRoot(HCkCacheW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC int CkCacheW_DeleteAll(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC int CkCacheW_DeleteAllExpired(HCkCacheW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCacheW_DeleteFromCache(HCkCacheW cHandle, const wchar_t *key);
CK_VISIBLE_PUBLIC int CkCacheW_DeleteOlder(HCkCacheW cHandle, SYSTEMTIME *dateTime);
CK_VISIBLE_PUBLIC int CkCacheW_DeleteOlderDt(HCkCacheW cHandle, HCkDateTimeW dateTime);
CK_VISIBLE_PUBLIC int CkCacheW_DeleteOlderStr(HCkCacheW cHandle, const wchar_t *dateTimeStr);
CK_VISIBLE_PUBLIC BOOL CkCacheW_FetchFromCache(HCkCacheW cHandle, const wchar_t *key, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkCacheW_FetchText(HCkCacheW cHandle, const wchar_t *key, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_fetchText(HCkCacheW cHandle, const wchar_t *key);
CK_VISIBLE_PUBLIC BOOL CkCacheW_GetEtag(HCkCacheW cHandle, const wchar_t *key, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_getEtag(HCkCacheW cHandle, const wchar_t *key);
CK_VISIBLE_PUBLIC BOOL CkCacheW_GetExpiration(HCkCacheW cHandle, const wchar_t *key, SYSTEMTIME *outSysTime);
CK_VISIBLE_PUBLIC HCkDateTimeW CkCacheW_GetExpirationDt(HCkCacheW cHandle, const wchar_t *key);
CK_VISIBLE_PUBLIC BOOL CkCacheW_GetExpirationStr(HCkCacheW cHandle, const wchar_t *url, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_getExpirationStr(HCkCacheW cHandle, const wchar_t *url);
CK_VISIBLE_PUBLIC BOOL CkCacheW_GetFilename(HCkCacheW cHandle, const wchar_t *key, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_getFilename(HCkCacheW cHandle, const wchar_t *key);
CK_VISIBLE_PUBLIC BOOL CkCacheW_GetRoot(HCkCacheW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCacheW_getRoot(HCkCacheW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCacheW_IsCached(HCkCacheW cHandle, const wchar_t *key);
CK_VISIBLE_PUBLIC BOOL CkCacheW_SaveLastError(HCkCacheW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkCacheW_SaveText(HCkCacheW cHandle, const wchar_t *key, SYSTEMTIME *expireDateTime, const wchar_t *eTag, const wchar_t *itemTextData);
CK_VISIBLE_PUBLIC BOOL CkCacheW_SaveTextDt(HCkCacheW cHandle, const wchar_t *key, HCkDateTimeW expireDateTime, const wchar_t *eTag, const wchar_t *itemTextData);
CK_VISIBLE_PUBLIC BOOL CkCacheW_SaveTextNoExpire(HCkCacheW cHandle, const wchar_t *key, const wchar_t *eTag, const wchar_t *itemTextData);
CK_VISIBLE_PUBLIC BOOL CkCacheW_SaveTextStr(HCkCacheW cHandle, const wchar_t *key, const wchar_t *expireDateTime, const wchar_t *eTag, const wchar_t *itemTextData);
CK_VISIBLE_PUBLIC BOOL CkCacheW_SaveToCache(HCkCacheW cHandle, const wchar_t *key, SYSTEMTIME *expireDateTime, const wchar_t *eTag, HCkByteData itemData);
CK_VISIBLE_PUBLIC BOOL CkCacheW_SaveToCacheDt(HCkCacheW cHandle, const wchar_t *key, HCkDateTimeW expireDateTime, const wchar_t *eTag, HCkByteData itemData);
CK_VISIBLE_PUBLIC BOOL CkCacheW_SaveToCacheNoExpire(HCkCacheW cHandle, const wchar_t *key, const wchar_t *eTag, HCkByteData itemData);
CK_VISIBLE_PUBLIC BOOL CkCacheW_SaveToCacheStr(HCkCacheW cHandle, const wchar_t *key, const wchar_t *expireDateTime, const wchar_t *eTag, HCkByteData itemData);
CK_VISIBLE_PUBLIC BOOL CkCacheW_UpdateExpiration(HCkCacheW cHandle, const wchar_t *key, SYSTEMTIME *expireDateTime);
CK_VISIBLE_PUBLIC BOOL CkCacheW_UpdateExpirationDt(HCkCacheW cHandle, const wchar_t *key, HCkDateTimeW expireDateTime);
CK_VISIBLE_PUBLIC BOOL CkCacheW_UpdateExpirationStr(HCkCacheW cHandle, const wchar_t *key, const wchar_t *expireDateTime);
#endif
