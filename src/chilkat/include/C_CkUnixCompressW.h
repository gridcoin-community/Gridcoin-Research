// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkUnixCompressWH
#define _C_CkUnixCompressWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkUnixCompressW CkUnixCompressW_Create(void);
CK_VISIBLE_PUBLIC HCkUnixCompressW CkUnixCompressW_Create2(BOOL bCallbackOwned);
CK_VISIBLE_PUBLIC void CkUnixCompressW_Dispose(HCkUnixCompressW handle);
CK_VISIBLE_PUBLIC void CkUnixCompressW_getDebugLogFilePath(HCkUnixCompressW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkUnixCompressW_putDebugLogFilePath(HCkUnixCompressW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkUnixCompressW_debugLogFilePath(HCkUnixCompressW cHandle);
CK_VISIBLE_PUBLIC int CkUnixCompressW_getHeartbeatMs(HCkUnixCompressW cHandle);
CK_VISIBLE_PUBLIC void CkUnixCompressW_putHeartbeatMs(HCkUnixCompressW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkUnixCompressW_getLastErrorHtml(HCkUnixCompressW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkUnixCompressW_lastErrorHtml(HCkUnixCompressW cHandle);
CK_VISIBLE_PUBLIC void CkUnixCompressW_getLastErrorText(HCkUnixCompressW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkUnixCompressW_lastErrorText(HCkUnixCompressW cHandle);
CK_VISIBLE_PUBLIC void CkUnixCompressW_getLastErrorXml(HCkUnixCompressW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkUnixCompressW_lastErrorXml(HCkUnixCompressW cHandle);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_getVerboseLogging(HCkUnixCompressW cHandle);
CK_VISIBLE_PUBLIC void CkUnixCompressW_putVerboseLogging(HCkUnixCompressW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkUnixCompressW_getVersion(HCkUnixCompressW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkUnixCompressW_version(HCkUnixCompressW cHandle);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_CompressFile(HCkUnixCompressW cHandle, const wchar_t *inFilename, const wchar_t *destPath);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_CompressFileToMem(HCkUnixCompressW cHandle, const wchar_t *inFilename, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_CompressMemToFile(HCkUnixCompressW cHandle, HCkByteData inData, const wchar_t *destPath);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_CompressMemory(HCkUnixCompressW cHandle, HCkByteData inData, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_CompressString(HCkUnixCompressW cHandle, const wchar_t *inStr, const wchar_t *charset, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_CompressStringToFile(HCkUnixCompressW cHandle, const wchar_t *inStr, const wchar_t *charset, const wchar_t *outPath);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_IsUnlocked(HCkUnixCompressW cHandle);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_SaveLastError(HCkUnixCompressW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_UnTarZ(HCkUnixCompressW cHandle, const wchar_t *zFilename, const wchar_t *destDir, BOOL bNoAbsolute);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_UncompressFile(HCkUnixCompressW cHandle, const wchar_t *inFilename, const wchar_t *destPath);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_UncompressFileToMem(HCkUnixCompressW cHandle, const wchar_t *inFilename, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_UncompressFileToString(HCkUnixCompressW cHandle, const wchar_t *zFilename, const wchar_t *charset, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkUnixCompressW_uncompressFileToString(HCkUnixCompressW cHandle, const wchar_t *zFilename, const wchar_t *charset);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_UncompressMemToFile(HCkUnixCompressW cHandle, HCkByteData inData, const wchar_t *destPath);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_UncompressMemory(HCkUnixCompressW cHandle, HCkByteData inData, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_UncompressString(HCkUnixCompressW cHandle, HCkByteData inCompressedData, const wchar_t *charset, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkUnixCompressW_uncompressString(HCkUnixCompressW cHandle, HCkByteData inCompressedData, const wchar_t *charset);
CK_VISIBLE_PUBLIC BOOL CkUnixCompressW_UnlockComponent(HCkUnixCompressW cHandle, const wchar_t *unlockCode);
#endif
