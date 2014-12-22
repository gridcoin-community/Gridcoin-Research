// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkSocksProxy_H
#define _C_CkSocksProxy_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkSocksProxy CkSocksProxy_Create(void);
CK_VISIBLE_PUBLIC void CkSocksProxy_Dispose(HCkSocksProxy handle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_getAllowUnauthenticatedSocks5(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_putAllowUnauthenticatedSocks5(HCkSocksProxy cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_getAuthenticatedSocks5(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_getClientIp(HCkSocksProxy cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_clientIp(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC int CkSocksProxy_getClientPort(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_getConnectionPending(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_getDebugLogFilePath(HCkSocksProxy cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkSocksProxy_putDebugLogFilePath(HCkSocksProxy cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_debugLogFilePath(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_getLastErrorHtml(HCkSocksProxy cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_lastErrorHtml(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_getLastErrorText(HCkSocksProxy cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_lastErrorText(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_getLastErrorXml(HCkSocksProxy cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_lastErrorXml(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_getListenBindIpAddress(HCkSocksProxy cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkSocksProxy_putListenBindIpAddress(HCkSocksProxy cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_listenBindIpAddress(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_getLogin(HCkSocksProxy cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_login(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_getOutboundBindIpAddress(HCkSocksProxy cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkSocksProxy_putOutboundBindIpAddress(HCkSocksProxy cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_outboundBindIpAddress(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC int CkSocksProxy_getOutboundBindPort(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_putOutboundBindPort(HCkSocksProxy cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkSocksProxy_getPassword(HCkSocksProxy cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_password(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_getServerIp(HCkSocksProxy cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_serverIp(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC int CkSocksProxy_getServerPort(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC int CkSocksProxy_getSocksVersion(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_getUtf8(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_putUtf8(HCkSocksProxy cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_getVerboseLogging(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC void CkSocksProxy_putVerboseLogging(HCkSocksProxy cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkSocksProxy_getVersion(HCkSocksProxy cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_version(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_AllowConnection(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_GetTunnelsXml(HCkSocksProxy cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkSocksProxy_getTunnelsXml(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_Initialize(HCkSocksProxy cHandle, int port);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_ProceedSocks5(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_RejectConnection(HCkSocksProxy cHandle);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_SaveLastError(HCkSocksProxy cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_StopAllTunnels(HCkSocksProxy cHandle, int maxWaitMs);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_UnlockComponent(HCkSocksProxy cHandle, const char *unlockCode);
CK_VISIBLE_PUBLIC BOOL CkSocksProxy_WaitForConnection(HCkSocksProxy cHandle, int maxWaitMs);
#endif
