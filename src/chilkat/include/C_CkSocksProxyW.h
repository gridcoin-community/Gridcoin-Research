// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkSocksProxyWH
#define _C_CkSocksProxyWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkSocksProxyW CkSocksProxyW_Create(void);
CK_VISIBLE_PUBLIC HCkSocksProxyW CkSocksProxyW_Create2(BOOL bCallbackOwned);
CK_VISIBLE_PUBLIC void CkSocksProxyW_Dispose(HCkSocksProxyW handle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_getAllowUnauthenticatedSocks5(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_putAllowUnauthenticatedSocks5(HCkSocksProxyW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_getAuthenticatedSocks5(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_getClientIp(HCkSocksProxyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_clientIp(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC int CkSocksProxyW_getClientPort(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_getConnectionPending(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_getDebugLogFilePath(HCkSocksProxyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkSocksProxyW_putDebugLogFilePath(HCkSocksProxyW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_debugLogFilePath(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_getLastErrorHtml(HCkSocksProxyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_lastErrorHtml(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_getLastErrorText(HCkSocksProxyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_lastErrorText(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_getLastErrorXml(HCkSocksProxyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_lastErrorXml(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_getListenBindIpAddress(HCkSocksProxyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkSocksProxyW_putListenBindIpAddress(HCkSocksProxyW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_listenBindIpAddress(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_getLogin(HCkSocksProxyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_login(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_getOutboundBindIpAddress(HCkSocksProxyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkSocksProxyW_putOutboundBindIpAddress(HCkSocksProxyW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_outboundBindIpAddress(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC int CkSocksProxyW_getOutboundBindPort(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_putOutboundBindPort(HCkSocksProxyW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkSocksProxyW_getPassword(HCkSocksProxyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_password(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_getServerIp(HCkSocksProxyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_serverIp(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC int CkSocksProxyW_getServerPort(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC int CkSocksProxyW_getSocksVersion(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_getVerboseLogging(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxyW_putVerboseLogging(HCkSocksProxyW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkSocksProxyW_getVersion(HCkSocksProxyW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_version(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_AllowConnection(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_GetTunnelsXml(HCkSocksProxyW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkSocksProxyW_getTunnelsXml(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_Initialize(HCkSocksProxyW cHandle, int port);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_ProceedSocks5(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_RejectConnection(HCkSocksProxyW cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_SaveLastError(HCkSocksProxyW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_StopAllTunnels(HCkSocksProxyW cHandle, int maxWaitMs);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_UnlockComponent(HCkSocksProxyW cHandle, const wchar_t *unlockCode);
CK_VISIBLE_PUBLIC BOOL CkSocksProxyW_WaitForConnection(HCkSocksProxyW cHandle, int maxWaitMs);
#endif
