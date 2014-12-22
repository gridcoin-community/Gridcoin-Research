// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkTarWH
#define _C_CkTarWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkTarW CkTarW_Create(void);
CK_VISIBLE_PUBLIC HCkTarW CkTarW_Create2(BOOL bCallbackOwned);
CK_VISIBLE_PUBLIC void CkTarW_Dispose(HCkTarW handle);
CK_VISIBLE_PUBLIC void CkTarW_getCharset(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTarW_putCharset(HCkTarW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_charset(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_getDebugLogFilePath(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTarW_putDebugLogFilePath(HCkTarW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_debugLogFilePath(HCkTarW cHandle);
CK_VISIBLE_PUBLIC int CkTarW_getDirMode(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putDirMode(HCkTarW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkTarW_getDirPrefix(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTarW_putDirPrefix(HCkTarW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_dirPrefix(HCkTarW cHandle);
CK_VISIBLE_PUBLIC int CkTarW_getFileMode(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putFileMode(HCkTarW cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkTarW_getGroupId(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putGroupId(HCkTarW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkTarW_getGroupName(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTarW_putGroupName(HCkTarW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_groupName(HCkTarW cHandle);
CK_VISIBLE_PUBLIC int CkTarW_getHeartbeatMs(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putHeartbeatMs(HCkTarW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkTarW_getLastErrorHtml(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_lastErrorHtml(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_getLastErrorText(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_lastErrorText(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_getLastErrorXml(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_lastErrorXml(HCkTarW cHandle);
CK_VISIBLE_PUBLIC BOOL CkTarW_getNoAbsolutePaths(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putNoAbsolutePaths(HCkTarW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC int CkTarW_getNumDirRoots(HCkTarW cHandle);
CK_VISIBLE_PUBLIC int CkTarW_getScriptFileMode(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putScriptFileMode(HCkTarW cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkTarW_getUntarCaseSensitive(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putUntarCaseSensitive(HCkTarW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkTarW_getUntarDebugLog(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putUntarDebugLog(HCkTarW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkTarW_getUntarDiscardPaths(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putUntarDiscardPaths(HCkTarW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkTarW_getUntarFromDir(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTarW_putUntarFromDir(HCkTarW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_untarFromDir(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_getUntarMatchPattern(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTarW_putUntarMatchPattern(HCkTarW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_untarMatchPattern(HCkTarW cHandle);
CK_VISIBLE_PUBLIC int CkTarW_getUntarMaxCount(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putUntarMaxCount(HCkTarW cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkTarW_getUserId(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putUserId(HCkTarW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkTarW_getUserName(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTarW_putUserName(HCkTarW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_userName(HCkTarW cHandle);
CK_VISIBLE_PUBLIC BOOL CkTarW_getVerboseLogging(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_putVerboseLogging(HCkTarW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkTarW_getVersion(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_version(HCkTarW cHandle);
CK_VISIBLE_PUBLIC void CkTarW_getWriteFormat(HCkTarW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTarW_putWriteFormat(HCkTarW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_writeFormat(HCkTarW cHandle);
CK_VISIBLE_PUBLIC BOOL CkTarW_AddDirRoot(HCkTarW cHandle, const wchar_t *dirPath);
CK_VISIBLE_PUBLIC BOOL CkTarW_GetDirRoot(HCkTarW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_getDirRoot(HCkTarW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkTarW_ListXml(HCkTarW cHandle, const wchar_t *tarPath, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkTarW_listXml(HCkTarW cHandle, const wchar_t *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTarW_SaveLastError(HCkTarW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkTarW_UnlockComponent(HCkTarW cHandle, const wchar_t *unlockCode);
CK_VISIBLE_PUBLIC int CkTarW_Untar(HCkTarW cHandle, const wchar_t *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTarW_UntarBz2(HCkTarW cHandle, const wchar_t *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTarW_UntarFirstMatchingToMemory(HCkTarW cHandle, HCkByteData tarFileBytes, const wchar_t *matchPattern, HCkByteData outBytes);
CK_VISIBLE_PUBLIC int CkTarW_UntarFromMemory(HCkTarW cHandle, HCkByteData tarFileBytes);
CK_VISIBLE_PUBLIC BOOL CkTarW_UntarGz(HCkTarW cHandle, const wchar_t *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTarW_UntarZ(HCkTarW cHandle, const wchar_t *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTarW_VerifyTar(HCkTarW cHandle, const wchar_t *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTarW_WriteTar(HCkTarW cHandle, const wchar_t *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTarW_WriteTarBz2(HCkTarW cHandle, const wchar_t *bz2Path);
CK_VISIBLE_PUBLIC BOOL CkTarW_WriteTarGz(HCkTarW cHandle, const wchar_t *gzPath);
#endif
