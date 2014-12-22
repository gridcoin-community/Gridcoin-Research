// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkNtlmWH
#define _C_CkNtlmWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkNtlmW CkNtlmW_Create(void);
CK_VISIBLE_PUBLIC void CkNtlmW_Dispose(HCkNtlmW handle);
CK_VISIBLE_PUBLIC void CkNtlmW_getClientChallenge(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putClientChallenge(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_clientChallenge(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getDebugLogFilePath(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putDebugLogFilePath(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_debugLogFilePath(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getDnsComputerName(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putDnsComputerName(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_dnsComputerName(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getDnsDomainName(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putDnsDomainName(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_dnsDomainName(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getDomain(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putDomain(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_domain(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getEncodingMode(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putEncodingMode(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_encodingMode(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getFlags(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putFlags(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_flags(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getLastErrorHtml(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_lastErrorHtml(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getLastErrorText(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_lastErrorText(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getLastErrorXml(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_lastErrorXml(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getNetBiosComputerName(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putNetBiosComputerName(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_netBiosComputerName(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getNetBiosDomainName(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putNetBiosDomainName(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_netBiosDomainName(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC int CkNtlmW_getNtlmVersion(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_putNtlmVersion(HCkNtlmW cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkNtlmW_getOemCodePage(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_putOemCodePage(HCkNtlmW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkNtlmW_getPassword(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putPassword(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_password(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getServerChallenge(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putServerChallenge(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_serverChallenge(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getTargetName(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putTargetName(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_targetName(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getUserName(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putUserName(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_userName(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_getVerboseLogging(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_putVerboseLogging(HCkNtlmW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkNtlmW_getVersion(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_version(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC void CkNtlmW_getWorkstation(HCkNtlmW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlmW_putWorkstation(HCkNtlmW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_workstation(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_CompareType3(HCkNtlmW cHandle, const wchar_t *msg1, const wchar_t *msg2);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_GenType1(HCkNtlmW cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_genType1(HCkNtlmW cHandle);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_GenType2(HCkNtlmW cHandle, const wchar_t *type1Msg, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_genType2(HCkNtlmW cHandle, const wchar_t *type1Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_GenType3(HCkNtlmW cHandle, const wchar_t *type2Msg, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_genType3(HCkNtlmW cHandle, const wchar_t *type2Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_LoadType3(HCkNtlmW cHandle, const wchar_t *type3Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_ParseType1(HCkNtlmW cHandle, const wchar_t *type1Msg, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_parseType1(HCkNtlmW cHandle, const wchar_t *type1Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_ParseType2(HCkNtlmW cHandle, const wchar_t *type2Msg, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_parseType2(HCkNtlmW cHandle, const wchar_t *type2Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_ParseType3(HCkNtlmW cHandle, const wchar_t *type3Msg, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkNtlmW_parseType3(HCkNtlmW cHandle, const wchar_t *type3Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_SaveLastError(HCkNtlmW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_SetFlag(HCkNtlmW cHandle, const wchar_t *flagLetter, BOOL onOrOff);
CK_VISIBLE_PUBLIC BOOL CkNtlmW_UnlockComponent(HCkNtlmW cHandle, const wchar_t *unlockCode);
#endif
