// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkTar_H
#define _C_CkTar_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkTar CkTar_Create(void);
CK_VISIBLE_PUBLIC void CkTar_Dispose(HCkTar handle);
CK_VISIBLE_PUBLIC void CkTar_getCharset(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTar_putCharset(HCkTar cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkTar_charset(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_getDebugLogFilePath(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTar_putDebugLogFilePath(HCkTar cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkTar_debugLogFilePath(HCkTar cHandle);
CK_VISIBLE_PUBLIC int CkTar_getDirMode(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putDirMode(HCkTar cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkTar_getDirPrefix(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTar_putDirPrefix(HCkTar cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkTar_dirPrefix(HCkTar cHandle);
CK_VISIBLE_PUBLIC int CkTar_getFileMode(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putFileMode(HCkTar cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkTar_getGroupId(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putGroupId(HCkTar cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkTar_getGroupName(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTar_putGroupName(HCkTar cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkTar_groupName(HCkTar cHandle);
CK_VISIBLE_PUBLIC int CkTar_getHeartbeatMs(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putHeartbeatMs(HCkTar cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkTar_getLastErrorHtml(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkTar_lastErrorHtml(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_getLastErrorText(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkTar_lastErrorText(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_getLastErrorXml(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkTar_lastErrorXml(HCkTar cHandle);
CK_VISIBLE_PUBLIC BOOL CkTar_getNoAbsolutePaths(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putNoAbsolutePaths(HCkTar cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC int CkTar_getNumDirRoots(HCkTar cHandle);
CK_VISIBLE_PUBLIC int CkTar_getScriptFileMode(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putScriptFileMode(HCkTar cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkTar_getUntarCaseSensitive(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putUntarCaseSensitive(HCkTar cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkTar_getUntarDebugLog(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putUntarDebugLog(HCkTar cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkTar_getUntarDiscardPaths(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putUntarDiscardPaths(HCkTar cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkTar_getUntarFromDir(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTar_putUntarFromDir(HCkTar cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkTar_untarFromDir(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_getUntarMatchPattern(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTar_putUntarMatchPattern(HCkTar cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkTar_untarMatchPattern(HCkTar cHandle);
CK_VISIBLE_PUBLIC int CkTar_getUntarMaxCount(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putUntarMaxCount(HCkTar cHandle, int newVal);
CK_VISIBLE_PUBLIC int CkTar_getUserId(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putUserId(HCkTar cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkTar_getUserName(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTar_putUserName(HCkTar cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkTar_userName(HCkTar cHandle);
CK_VISIBLE_PUBLIC BOOL CkTar_getUtf8(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putUtf8(HCkTar cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkTar_getVerboseLogging(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_putVerboseLogging(HCkTar cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkTar_getVersion(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkTar_version(HCkTar cHandle);
CK_VISIBLE_PUBLIC void CkTar_getWriteFormat(HCkTar cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkTar_putWriteFormat(HCkTar cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkTar_writeFormat(HCkTar cHandle);
CK_VISIBLE_PUBLIC BOOL CkTar_AddDirRoot(HCkTar cHandle, const char *dirPath);
CK_VISIBLE_PUBLIC BOOL CkTar_GetDirRoot(HCkTar cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkTar_getDirRoot(HCkTar cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkTar_ListXml(HCkTar cHandle, const char *tarPath, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkTar_listXml(HCkTar cHandle, const char *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTar_SaveLastError(HCkTar cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkTar_UnlockComponent(HCkTar cHandle, const char *unlockCode);
CK_VISIBLE_PUBLIC int CkTar_Untar(HCkTar cHandle, const char *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTar_UntarBz2(HCkTar cHandle, const char *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTar_UntarFirstMatchingToMemory(HCkTar cHandle, HCkByteData tarFileBytes, const char *matchPattern, HCkByteData outBytes);
CK_VISIBLE_PUBLIC int CkTar_UntarFromMemory(HCkTar cHandle, HCkByteData tarFileBytes);
CK_VISIBLE_PUBLIC BOOL CkTar_UntarGz(HCkTar cHandle, const char *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTar_UntarZ(HCkTar cHandle, const char *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTar_VerifyTar(HCkTar cHandle, const char *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTar_WriteTar(HCkTar cHandle, const char *tarPath);
CK_VISIBLE_PUBLIC BOOL CkTar_WriteTarBz2(HCkTar cHandle, const char *bz2Path);
CK_VISIBLE_PUBLIC BOOL CkTar_WriteTarGz(HCkTar cHandle, const char *gzPath);
#endif
