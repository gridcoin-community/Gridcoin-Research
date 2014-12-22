// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkNtlm_H
#define _C_CkNtlm_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkNtlm CkNtlm_Create(void);
CK_VISIBLE_PUBLIC void CkNtlm_Dispose(HCkNtlm handle);
CK_VISIBLE_PUBLIC void CkNtlm_getClientChallenge(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putClientChallenge(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_clientChallenge(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getDebugLogFilePath(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putDebugLogFilePath(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_debugLogFilePath(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getDnsComputerName(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putDnsComputerName(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_dnsComputerName(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getDnsDomainName(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putDnsDomainName(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_dnsDomainName(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getDomain(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putDomain(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_domain(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getEncodingMode(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putEncodingMode(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_encodingMode(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getFlags(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putFlags(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_flags(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getLastErrorHtml(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkNtlm_lastErrorHtml(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getLastErrorText(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkNtlm_lastErrorText(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getLastErrorXml(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkNtlm_lastErrorXml(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getNetBiosComputerName(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putNetBiosComputerName(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_netBiosComputerName(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getNetBiosDomainName(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putNetBiosDomainName(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_netBiosDomainName(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC int CkNtlm_getNtlmVersion(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_putNtlmVersion(HCkNtlm cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkNtlm_getOemCodePage(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_putOemCodePage(HCkNtlm cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkNtlm_getPassword(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putPassword(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_password(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getServerChallenge(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putServerChallenge(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_serverChallenge(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getTargetName(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putTargetName(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_targetName(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getUserName(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putUserName(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_userName(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC BOOL CkNtlm_getUtf8(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_putUtf8(HCkNtlm cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkNtlm_getVerboseLogging(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_putVerboseLogging(HCkNtlm cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkNtlm_getVersion(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkNtlm_version(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC void CkNtlm_getWorkstation(HCkNtlm cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkNtlm_putWorkstation(HCkNtlm cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkNtlm_workstation(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC BOOL CkNtlm_CompareType3(HCkNtlm cHandle, const char *msg1, const char *msg2);
CK_VISIBLE_PUBLIC BOOL CkNtlm_GenType1(HCkNtlm cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkNtlm_genType1(HCkNtlm cHandle);
CK_VISIBLE_PUBLIC BOOL CkNtlm_GenType2(HCkNtlm cHandle, const char *type1Msg, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkNtlm_genType2(HCkNtlm cHandle, const char *type1Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlm_GenType3(HCkNtlm cHandle, const char *type2Msg, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkNtlm_genType3(HCkNtlm cHandle, const char *type2Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlm_LoadType3(HCkNtlm cHandle, const char *type3Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlm_ParseType1(HCkNtlm cHandle, const char *type1Msg, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkNtlm_parseType1(HCkNtlm cHandle, const char *type1Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlm_ParseType2(HCkNtlm cHandle, const char *type2Msg, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkNtlm_parseType2(HCkNtlm cHandle, const char *type2Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlm_ParseType3(HCkNtlm cHandle, const char *type3Msg, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkNtlm_parseType3(HCkNtlm cHandle, const char *type3Msg);
CK_VISIBLE_PUBLIC BOOL CkNtlm_SaveLastError(HCkNtlm cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkNtlm_SetFlag(HCkNtlm cHandle, const char *flagLetter, BOOL onOrOff);
CK_VISIBLE_PUBLIC BOOL CkNtlm_UnlockComponent(HCkNtlm cHandle, const char *unlockCode);
#endif
