// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCertStoreWH
#define _C_CkCertStoreWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCertStoreW CkCertStoreW_Create(void);
CK_VISIBLE_PUBLIC void CkCertStoreW_Dispose(HCkCertStoreW handle);
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_getAvoidWindowsPkAccess(HCkCertStoreW cHandle);
CK_VISIBLE_PUBLIC void CkCertStoreW_putAvoidWindowsPkAccess(HCkCertStoreW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCertStoreW_getDebugLogFilePath(HCkCertStoreW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCertStoreW_putDebugLogFilePath(HCkCertStoreW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCertStoreW_debugLogFilePath(HCkCertStoreW cHandle);
CK_VISIBLE_PUBLIC void CkCertStoreW_getLastErrorHtml(HCkCertStoreW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertStoreW_lastErrorHtml(HCkCertStoreW cHandle);
CK_VISIBLE_PUBLIC void CkCertStoreW_getLastErrorText(HCkCertStoreW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertStoreW_lastErrorText(HCkCertStoreW cHandle);
CK_VISIBLE_PUBLIC void CkCertStoreW_getLastErrorXml(HCkCertStoreW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertStoreW_lastErrorXml(HCkCertStoreW cHandle);
CK_VISIBLE_PUBLIC int CkCertStoreW_getNumCertificates(HCkCertStoreW cHandle);
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC int CkCertStoreW_getNumEmailCerts(HCkCertStoreW cHandle);
#endif
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_getVerboseLogging(HCkCertStoreW cHandle);
CK_VISIBLE_PUBLIC void CkCertStoreW_putVerboseLogging(HCkCertStoreW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCertStoreW_getVersion(HCkCertStoreW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCertStoreW_version(HCkCertStoreW cHandle);
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_AddCertificate(HCkCertStoreW cHandle, HCkCertW cert);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_CreateFileStore(HCkCertStoreW cHandle, const wchar_t *filename);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_CreateMemoryStore(HCkCertStoreW cHandle);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_CreateRegistryStore(HCkCertStoreW cHandle, const wchar_t *regRoot, const wchar_t *regPath);
#endif
CK_VISIBLE_PUBLIC HCkCertW CkCertStoreW_FindCertByRfc822Name(HCkCertStoreW cHandle, const wchar_t *name);
CK_VISIBLE_PUBLIC HCkCertW CkCertStoreW_FindCertBySerial(HCkCertStoreW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC HCkCertW CkCertStoreW_FindCertBySha1Thumbprint(HCkCertStoreW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC HCkCertW CkCertStoreW_FindCertBySubject(HCkCertStoreW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC HCkCertW CkCertStoreW_FindCertBySubjectCN(HCkCertStoreW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC HCkCertW CkCertStoreW_FindCertBySubjectE(HCkCertStoreW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC HCkCertW CkCertStoreW_FindCertBySubjectO(HCkCertStoreW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC HCkCertW CkCertStoreW_FindCertForEmail(HCkCertStoreW cHandle, const wchar_t *emailAddress);
CK_VISIBLE_PUBLIC HCkCertW CkCertStoreW_GetCertificate(HCkCertStoreW cHandle, int index);
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC HCkCertW CkCertStoreW_GetEmailCert(HCkCertStoreW cHandle, int index);
#endif
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_LoadPemFile(HCkCertStoreW cHandle, const wchar_t *pemPath);
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_LoadPemStr(HCkCertStoreW cHandle, const wchar_t *pemString);
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_LoadPfxData(HCkCertStoreW cHandle, HCkByteData pfxData, const wchar_t *password);
#if !defined(CHILKAT_MONO)
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_LoadPfxData2(HCkCertStoreW cHandle, const unsigned char *pByteData, unsigned long szByteData, const wchar_t *password);
#endif
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_LoadPfxFile(HCkCertStoreW cHandle, const wchar_t *pfxFilename, const wchar_t *password);
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_OpenCurrentUserStore(HCkCertStoreW cHandle, BOOL readOnly);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_OpenFileStore(HCkCertStoreW cHandle, const wchar_t *filename, BOOL readOnly);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_OpenLocalSystemStore(HCkCertStoreW cHandle, BOOL readOnly);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_OpenRegistryStore(HCkCertStoreW cHandle, const wchar_t *regRoot, const wchar_t *regPath, BOOL readOnly);
#endif
#if defined(CK_WINCERTSTORE_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_RemoveCertificate(HCkCertStoreW cHandle, HCkCertW cert);
#endif
CK_VISIBLE_PUBLIC BOOL CkCertStoreW_SaveLastError(HCkCertStoreW cHandle, const wchar_t *path);
#endif
