// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCache_H
#define _C_CkCache_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCache CkCache_Create(void);
CK_VISIBLE_PUBLIC void CkCache_Dispose(HCkCache handle);
CK_VISIBLE_PUBLIC void CkCache_getDebugLogFilePath(HCkCache cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCache_putDebugLogFilePath(HCkCache cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCache_debugLogFilePath(HCkCache cHandle);
CK_VISIBLE_PUBLIC void CkCache_getLastErrorHtml(HCkCache cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCache_lastErrorHtml(HCkCache cHandle);
CK_VISIBLE_PUBLIC void CkCache_getLastErrorText(HCkCache cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCache_lastErrorText(HCkCache cHandle);
CK_VISIBLE_PUBLIC void CkCache_getLastErrorXml(HCkCache cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCache_lastErrorXml(HCkCache cHandle);
CK_VISIBLE_PUBLIC void CkCache_getLastEtagFetched(HCkCache cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCache_lastEtagFetched(HCkCache cHandle);
CK_VISIBLE_PUBLIC void CkCache_getLastExpirationFetched(HCkCache cHandle, SYSTEMTIME *retval);
CK_VISIBLE_PUBLIC void CkCache_getLastExpirationFetchedStr(HCkCache cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCache_lastExpirationFetchedStr(HCkCache cHandle);
CK_VISIBLE_PUBLIC BOOL CkCache_getLastHitExpired(HCkCache cHandle);
CK_VISIBLE_PUBLIC void CkCache_getLastKeyFetched(HCkCache cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCache_lastKeyFetched(HCkCache cHandle);
CK_VISIBLE_PUBLIC int CkCache_getLevel(HCkCache cHandle);
CK_VISIBLE_PUBLIC void CkCache_putLevel(HCkCache cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkCache_getNumRoots(HCkCache cHandle);
CK_VISIBLE_PUBLIC BOOL CkCache_getUtf8(HCkCache cHandle);
CK_VISIBLE_PUBLIC void CkCache_putUtf8(HCkCache cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkCache_getVerboseLogging(HCkCache cHandle);
CK_VISIBLE_PUBLIC void CkCache_putVerboseLogging(HCkCache cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCache_getVersion(HCkCache cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCache_version(HCkCache cHandle);
CK_VISIBLE_PUBLIC void CkCache_AddRoot(HCkCache cHandle, const char *path);
CK_VISIBLE_PUBLIC int CkCache_DeleteAll(HCkCache cHandle);
CK_VISIBLE_PUBLIC int CkCache_DeleteAllExpired(HCkCache cHandle);
CK_VISIBLE_PUBLIC BOOL CkCache_DeleteFromCache(HCkCache cHandle, const char *key);
CK_VISIBLE_PUBLIC int CkCache_DeleteOlder(HCkCache cHandle, SYSTEMTIME *dateTime);
CK_VISIBLE_PUBLIC int CkCache_DeleteOlderDt(HCkCache cHandle, HCkDateTime dateTime);
CK_VISIBLE_PUBLIC int CkCache_DeleteOlderStr(HCkCache cHandle, const char *dateTimeStr);
CK_VISIBLE_PUBLIC BOOL CkCache_FetchFromCache(HCkCache cHandle, const char *key, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkCache_FetchText(HCkCache cHandle, const char *key, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCache_fetchText(HCkCache cHandle, const char *key);
CK_VISIBLE_PUBLIC BOOL CkCache_GetEtag(HCkCache cHandle, const char *key, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCache_getEtag(HCkCache cHandle, const char *key);
CK_VISIBLE_PUBLIC BOOL CkCache_GetExpiration(HCkCache cHandle, const char *key, SYSTEMTIME *outSysTime);
CK_VISIBLE_PUBLIC HCkDateTime CkCache_GetExpirationDt(HCkCache cHandle, const char *key);
CK_VISIBLE_PUBLIC BOOL CkCache_GetExpirationStr(HCkCache cHandle, const char *url, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCache_getExpirationStr(HCkCache cHandle, const char *url);
CK_VISIBLE_PUBLIC BOOL CkCache_GetFilename(HCkCache cHandle, const char *key, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCache_getFilename(HCkCache cHandle, const char *key);
CK_VISIBLE_PUBLIC BOOL CkCache_GetRoot(HCkCache cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCache_getRoot(HCkCache cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCache_IsCached(HCkCache cHandle, const char *key);
CK_VISIBLE_PUBLIC BOOL CkCache_SaveLastError(HCkCache cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCache_SaveText(HCkCache cHandle, const char *key, SYSTEMTIME *expireDateTime, const char *eTag, const char *itemTextData);
CK_VISIBLE_PUBLIC BOOL CkCache_SaveTextDt(HCkCache cHandle, const char *key, HCkDateTime expireDateTime, const char *eTag, const char *itemTextData);
CK_VISIBLE_PUBLIC BOOL CkCache_SaveTextNoExpire(HCkCache cHandle, const char *key, const char *eTag, const char *itemTextData);
CK_VISIBLE_PUBLIC BOOL CkCache_SaveTextStr(HCkCache cHandle, const char *key, const char *expireDateTime, const char *eTag, const char *itemTextData);
CK_VISIBLE_PUBLIC BOOL CkCache_SaveToCache(HCkCache cHandle, const char *key, SYSTEMTIME *expireDateTime, const char *eTag, HCkByteData itemData);
CK_VISIBLE_PUBLIC BOOL CkCache_SaveToCacheDt(HCkCache cHandle, const char *key, HCkDateTime expireDateTime, const char *eTag, HCkByteData itemData);
CK_VISIBLE_PUBLIC BOOL CkCache_SaveToCacheNoExpire(HCkCache cHandle, const char *key, const char *eTag, HCkByteData itemData);
CK_VISIBLE_PUBLIC BOOL CkCache_SaveToCacheStr(HCkCache cHandle, const char *key, const char *expireDateTime, const char *eTag, HCkByteData itemData);
CK_VISIBLE_PUBLIC BOOL CkCache_UpdateExpiration(HCkCache cHandle, const char *key, SYSTEMTIME *expireDateTime);
CK_VISIBLE_PUBLIC BOOL CkCache_UpdateExpirationDt(HCkCache cHandle, const char *key, HCkDateTime expireDateTime);
CK_VISIBLE_PUBLIC BOOL CkCache_UpdateExpirationStr(HCkCache cHandle, const char *key, const char *expireDateTime);
#endif
