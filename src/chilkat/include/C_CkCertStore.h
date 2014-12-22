// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCertStore_H
#define _C_CkCertStore_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCertStore CkCertStore_Create(void);
CK_VISIBLE_PUBLIC void CkCertStore_Dispose(HCkCertStore handle);
CK_VISIBLE_PUBLIC BOOL CkCertStore_getAvoidWindowsPkAccess(HCkCertStore cHandle);
CK_VISIBLE_PUBLIC void CkCertStore_putAvoidWindowsPkAccess(HCkCertStore cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCertStore_getDebugLogFilePath(HCkCertStore cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCertStore_putDebugLogFilePath(HCkCertStore cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCertStore_debugLogFilePath(HCkCertStore cHandle);
CK_VISIBLE_PUBLIC void CkCertStore_getLastErrorHtml(HCkCertStore cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCertStore_lastErrorHtml(HCkCertStore cHandle);
CK_VISIBLE_PUBLIC void CkCertStore_getLastErrorText(HCkCertStore cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCertStore_lastErrorText(HCkCertStore cHandle);
CK_VISIBLE_PUBLIC void CkCertStore_getLastErrorXml(HCkCertStore cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCertStore_lastErrorXml(HCkCertStore cHandle);
CK_VISIBLE_PUBLIC int CkCertStore_getNumCertificates(HCkCertStore cHandle);
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC int CkCertStore_getNumEmailCerts(HCkCertStore cHandle);
#endif
CK_VISIBLE_PUBLIC BOOL CkCertStore_getUtf8(HCkCertStore cHandle);
CK_VISIBLE_PUBLIC void CkCertStore_putUtf8(HCkCertStore cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkCertStore_getVerboseLogging(HCkCertStore cHandle);
CK_VISIBLE_PUBLIC void CkCertStore_putVerboseLogging(HCkCertStore cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCertStore_getVersion(HCkCertStore cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCertStore_version(HCkCertStore cHandle);
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStore_AddCertificate(HCkCertStore cHandle, HCkCert cert);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStore_CreateFileStore(HCkCertStore cHandle, const char *filename);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStore_CreateMemoryStore(HCkCertStore cHandle);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStore_CreateRegistryStore(HCkCertStore cHandle, const char *regRoot, const char *regPath);
#endif
CK_VISIBLE_PUBLIC HCkCert CkCertStore_FindCertByRfc822Name(HCkCertStore cHandle, const char *name);
CK_VISIBLE_PUBLIC HCkCert CkCertStore_FindCertBySerial(HCkCertStore cHandle, const char *str);
CK_VISIBLE_PUBLIC HCkCert CkCertStore_FindCertBySha1Thumbprint(HCkCertStore cHandle, const char *str);
CK_VISIBLE_PUBLIC HCkCert CkCertStore_FindCertBySubject(HCkCertStore cHandle, const char *str);
CK_VISIBLE_PUBLIC HCkCert CkCertStore_FindCertBySubjectCN(HCkCertStore cHandle, const char *str);
CK_VISIBLE_PUBLIC HCkCert CkCertStore_FindCertBySubjectE(HCkCertStore cHandle, const char *str);
CK_VISIBLE_PUBLIC HCkCert CkCertStore_FindCertBySubjectO(HCkCertStore cHandle, const char *str);
CK_VISIBLE_PUBLIC HCkCert CkCertStore_FindCertForEmail(HCkCertStore cHandle, const char *emailAddress);
CK_VISIBLE_PUBLIC HCkCert CkCertStore_GetCertificate(HCkCertStore cHandle, int index);
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC HCkCert CkCertStore_GetEmailCert(HCkCertStore cHandle, int index);
#endif
CK_VISIBLE_PUBLIC BOOL CkCertStore_LoadPemFile(HCkCertStore cHandle, const char *pemPath);
CK_VISIBLE_PUBLIC BOOL CkCertStore_LoadPemStr(HCkCertStore cHandle, const char *pemString);
CK_VISIBLE_PUBLIC BOOL CkCertStore_LoadPfxData(HCkCertStore cHandle, HCkByteData pfxData, const char *password);
#if !defined(CHILKAT_MONO)
CK_VISIBLE_PUBLIC BOOL CkCertStore_LoadPfxData2(HCkCertStore cHandle, const unsigned char *pByteData, unsigned long szByteData, const char *password);
#endif
CK_VISIBLE_PUBLIC BOOL CkCertStore_LoadPfxFile(HCkCertStore cHandle, const char *pfxFilename, const char *password);
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStore_OpenCurrentUserStore(HCkCertStore cHandle, BOOL readOnly);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStore_OpenFileStore(HCkCertStore cHandle, const char *filename, BOOL readOnly);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStore_OpenLocalSystemStore(HCkCertStore cHandle, BOOL readOnly);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStore_OpenRegistryStore(HCkCertStore cHandle, const char *regRoot, const char *regPath, BOOL readOnly);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStore_RemoveCertificate(HCkCertStore cHandle, HCkCert cert);
#endif
CK_VISIBLE_PUBLIC BOOL CkCertStore_SaveLastError(HCkCertStore cHandle, const char *path);
#endif
