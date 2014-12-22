// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkZipWH
#define _C_CkZipWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkZipW CkZipW_Create(void);
CK_VISIBLE_PUBLIC HCkZipW CkZipW_Create2(BOOL bCallbackOwned);
CK_VISIBLE_PUBLIC void CkZipW_Dispose(HCkZipW handle);
CK_VISIBLE_PUBLIC void CkZipW_getAppendFromDir(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipW_putAppendFromDir(HCkZipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_appendFromDir(HCkZipW cHandle);
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_getAutoRun(HCkZipW cHandle, HCkString retval);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putAutoRun(HCkZipW cHandle, const wchar_t *newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_autoRun(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_getAutoRunParams(HCkZipW cHandle, HCkString retval);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putAutoRunParams(HCkZipW cHandle, const wchar_t *newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_autoRunParams(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_getAutoTemp(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putAutoTemp(HCkZipW cHandle, BOOL newVal);
#endif
CK_VISIBLE_PUBLIC BOOL CkZipW_getCaseSensitive(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putCaseSensitive(HCkZipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkZipW_getClearArchiveAttribute(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putClearArchiveAttribute(HCkZipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkZipW_getClearReadOnlyAttr(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putClearReadOnlyAttr(HCkZipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkZipW_getComment(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipW_putComment(HCkZipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_comment(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_getDebugLogFilePath(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipW_putDebugLogFilePath(HCkZipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_debugLogFilePath(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_getDecryptPassword(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipW_putDecryptPassword(HCkZipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_decryptPassword(HCkZipW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipW_getDiscardPaths(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putDiscardPaths(HCkZipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC int CkZipW_getEncryptKeyLength(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putEncryptKeyLength(HCkZipW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkZipW_getEncryptPassword(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipW_putEncryptPassword(HCkZipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_encryptPassword(HCkZipW cHandle);
CK_VISIBLE_PUBLIC int CkZipW_getEncryption(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putEncryption(HCkZipW cHandle, int newVal);
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_getExeDefaultDir(HCkZipW cHandle, HCkString retval);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putExeDefaultDir(HCkZipW cHandle, const wchar_t *newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_exeDefaultDir(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_getExeFinishNotifier(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putExeFinishNotifier(HCkZipW cHandle, BOOL newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_getExeIconFile(HCkZipW cHandle, HCkString retval);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putExeIconFile(HCkZipW cHandle, const wchar_t *newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_exeIconFile(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_getExeNoInterface(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putExeNoInterface(HCkZipW cHandle, BOOL newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_getExeSilentProgress(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putExeSilentProgress(HCkZipW cHandle, BOOL newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_getExeTitle(HCkZipW cHandle, HCkString retval);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putExeTitle(HCkZipW cHandle, const wchar_t *newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_exeTitle(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_getExeUnzipCaption(HCkZipW cHandle, HCkString retval);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putExeUnzipCaption(HCkZipW cHandle, const wchar_t *newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_exeUnzipCaption(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_getExeUnzipDir(HCkZipW cHandle, HCkString retval);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putExeUnzipDir(HCkZipW cHandle, const wchar_t *newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_exeUnzipDir(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_getExeWaitForSetup(HCkZipW cHandle);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putExeWaitForSetup(HCkZipW cHandle, BOOL newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_getExeXmlConfig(HCkZipW cHandle, HCkString retval);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_putExeXmlConfig(HCkZipW cHandle, const wchar_t *newVal);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_exeXmlConfig(HCkZipW cHandle);
#endif
CK_VISIBLE_PUBLIC int CkZipW_getFileCount(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_getFileName(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipW_putFileName(HCkZipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_fileName(HCkZipW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipW_getHasZipFormatErrors(HCkZipW cHandle);
CK_VISIBLE_PUBLIC int CkZipW_getHeartbeatMs(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putHeartbeatMs(HCkZipW cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkZipW_getIgnoreAccessDenied(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putIgnoreAccessDenied(HCkZipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkZipW_getLastErrorHtml(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_lastErrorHtml(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_getLastErrorText(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_lastErrorText(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_getLastErrorXml(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_lastErrorXml(HCkZipW cHandle);
CK_VISIBLE_PUBLIC int CkZipW_getNumEntries(HCkZipW cHandle);
CK_VISIBLE_PUBLIC int CkZipW_getOemCodePage(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putOemCodePage(HCkZipW cHandle, int newVal);
CK_VISIBLE_PUBLIC BOOL CkZipW_getOverwriteExisting(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putOverwriteExisting(HCkZipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkZipW_getPasswordProtect(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putPasswordProtect(HCkZipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkZipW_getPathPrefix(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipW_putPathPrefix(HCkZipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_pathPrefix(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_getTempDir(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipW_putTempDir(HCkZipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_tempDir(HCkZipW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipW_getTextFlag(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putTextFlag(HCkZipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkZipW_getVerboseLogging(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putVerboseLogging(HCkZipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkZipW_getVersion(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_version(HCkZipW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipW_getZipx(HCkZipW cHandle);
CK_VISIBLE_PUBLIC void CkZipW_putZipx(HCkZipW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkZipW_getZipxDefaultAlg(HCkZipW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkZipW_putZipxDefaultAlg(HCkZipW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_zipxDefaultAlg(HCkZipW cHandle);
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_AddEmbedded(HCkZipW cHandle, const wchar_t *exeFilename, const wchar_t *resourceName, const wchar_t *zipFilename);
#endif
CK_VISIBLE_PUBLIC void CkZipW_AddNoCompressExtension(HCkZipW cHandle, const wchar_t *fileExtension);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_AppendBase64(HCkZipW cHandle, const wchar_t *fileName, const wchar_t *encodedCompressedData);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_AppendCompressed(HCkZipW cHandle, const wchar_t *filename, HCkByteData inData);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_AppendData(HCkZipW cHandle, const wchar_t *fileName, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkZipW_AppendFiles(HCkZipW cHandle, const wchar_t *filePattern, BOOL recurse);
CK_VISIBLE_PUBLIC BOOL CkZipW_AppendFilesEx(HCkZipW cHandle, const wchar_t *filePattern, BOOL recurse, BOOL saveExtraPath, BOOL archiveOnly, BOOL includeHidden, BOOL includeSystem);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_AppendHex(HCkZipW cHandle, const wchar_t *fileName, const wchar_t *encodedCompressedData);
CK_VISIBLE_PUBLIC BOOL CkZipW_AppendMultiple(HCkZipW cHandle, HCkStringArrayW fileSpecs, BOOL recurse);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_AppendNew(HCkZipW cHandle, const wchar_t *fileName);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_AppendNewDir(HCkZipW cHandle, const wchar_t *dirName);
CK_VISIBLE_PUBLIC BOOL CkZipW_AppendOneFileOrDir(HCkZipW cHandle, const wchar_t *fileOrDirPath, BOOL saveExtraPath);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_AppendString(HCkZipW cHandle, const wchar_t *internalZipFilepath, const wchar_t *textData);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_AppendString2(HCkZipW cHandle, const wchar_t *internalZipFilepath, const wchar_t *textData, const wchar_t *charset);
CK_VISIBLE_PUBLIC BOOL CkZipW_AppendZip(HCkZipW cHandle, const wchar_t *zipFileName);
CK_VISIBLE_PUBLIC void CkZipW_CloseZip(HCkZipW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipW_DeleteEntry(HCkZipW cHandle, HCkZipEntryW entry);
CK_VISIBLE_PUBLIC void CkZipW_ExcludeDir(HCkZipW cHandle, const wchar_t *dirName);
CK_VISIBLE_PUBLIC BOOL CkZipW_Extract(HCkZipW cHandle, const wchar_t *dirPath);
CK_VISIBLE_PUBLIC BOOL CkZipW_ExtractInto(HCkZipW cHandle, const wchar_t *dirPath);
CK_VISIBLE_PUBLIC BOOL CkZipW_ExtractMatching(HCkZipW cHandle, const wchar_t *dirPath, const wchar_t *pattern);
CK_VISIBLE_PUBLIC BOOL CkZipW_ExtractNewer(HCkZipW cHandle, const wchar_t *dirPath);
CK_VISIBLE_PUBLIC BOOL CkZipW_ExtractOne(HCkZipW cHandle, HCkZipEntryW entry, const wchar_t *dirPath);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_FirstEntry(HCkZipW cHandle);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_FirstMatchingEntry(HCkZipW cHandle, const wchar_t *pattern);
CK_VISIBLE_PUBLIC BOOL CkZipW_GetDirectoryAsXML(HCkZipW cHandle, HCkString outXml);
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_getDirectoryAsXML(HCkZipW cHandle);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_GetEntryByID(HCkZipW cHandle, int entryID);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_GetEntryByIndex(HCkZipW cHandle, int index);
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_GetEntryByName(HCkZipW cHandle, const wchar_t *entryName);
CK_VISIBLE_PUBLIC HCkStringArrayW CkZipW_GetExclusions(HCkZipW cHandle);
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_GetExeConfigParam(HCkZipW cHandle, const wchar_t *name, HCkString outStr);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC const wchar_t *CkZipW_getExeConfigParam(HCkZipW cHandle, const wchar_t *name);
#endif
CK_VISIBLE_PUBLIC HCkZipEntryW CkZipW_InsertNew(HCkZipW cHandle, const wchar_t *fileName, int beforeIndex);
CK_VISIBLE_PUBLIC BOOL CkZipW_IsNoCompressExtension(HCkZipW cHandle, const wchar_t *fileExtension);
CK_VISIBLE_PUBLIC BOOL CkZipW_IsPasswordProtected(HCkZipW cHandle, const wchar_t *zipFilename);
CK_VISIBLE_PUBLIC BOOL CkZipW_IsUnlocked(HCkZipW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipW_NewZip(HCkZipW cHandle, const wchar_t *zipFilePath);
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_OpenEmbedded(HCkZipW cHandle, const wchar_t *exeFilename, const wchar_t *resourceName);
#endif
CK_VISIBLE_PUBLIC BOOL CkZipW_OpenFromByteData(HCkZipW cHandle, HCkByteData byteData);
CK_VISIBLE_PUBLIC BOOL CkZipW_OpenFromMemory(HCkZipW cHandle, HCkByteData inData);
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_OpenMyEmbedded(HCkZipW cHandle, const wchar_t *resourceName);
#endif
CK_VISIBLE_PUBLIC BOOL CkZipW_OpenZip(HCkZipW cHandle, const wchar_t *ZipFileName);
CK_VISIBLE_PUBLIC BOOL CkZipW_QuickAppend(HCkZipW cHandle, const wchar_t *ZipFileName);
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_RemoveEmbedded(HCkZipW cHandle, const wchar_t *exeFilename, const wchar_t *resourceName);
#endif
CK_VISIBLE_PUBLIC void CkZipW_RemoveNoCompressExtension(HCkZipW cHandle, const wchar_t *fileExtension);
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_ReplaceEmbedded(HCkZipW cHandle, const wchar_t *exeFilename, const wchar_t *resourceName, const wchar_t *zipFilename);
#endif
CK_VISIBLE_PUBLIC BOOL CkZipW_SaveLastError(HCkZipW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC void CkZipW_SetCompressionLevel(HCkZipW cHandle, int level);
CK_VISIBLE_PUBLIC void CkZipW_SetExclusions(HCkZipW cHandle, HCkStringArrayW excludePatterns);
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC void CkZipW_SetExeConfigParam(HCkZipW cHandle, const wchar_t *paramName, const wchar_t *paramValue);
#endif
CK_VISIBLE_PUBLIC void CkZipW_SetPassword(HCkZipW cHandle, const wchar_t *password);
CK_VISIBLE_PUBLIC BOOL CkZipW_UnlockComponent(HCkZipW cHandle, const wchar_t *regCode);
CK_VISIBLE_PUBLIC int CkZipW_Unzip(HCkZipW cHandle, const wchar_t *dirPath);
CK_VISIBLE_PUBLIC int CkZipW_UnzipInto(HCkZipW cHandle, const wchar_t *dirPath);
CK_VISIBLE_PUBLIC int CkZipW_UnzipMatching(HCkZipW cHandle, const wchar_t *dirPath, const wchar_t *pattern, BOOL verbose);
CK_VISIBLE_PUBLIC int CkZipW_UnzipMatchingInto(HCkZipW cHandle, const wchar_t *dirPath, const wchar_t *pattern, BOOL verbose);
CK_VISIBLE_PUBLIC int CkZipW_UnzipNewer(HCkZipW cHandle, const wchar_t *dirPath);
CK_VISIBLE_PUBLIC BOOL CkZipW_VerifyPassword(HCkZipW cHandle);
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_WriteExe(HCkZipW cHandle, const wchar_t *exeFilename);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_WriteExe2(HCkZipW cHandle, const wchar_t *exePath, const wchar_t *destExePath, BOOL bAesEncrypt, int keyLength, const wchar_t *password);
#endif
#if defined(CK_SFX_INCLUDED)
CK_VISIBLE_PUBLIC BOOL CkZipW_WriteExeToMemory(HCkZipW cHandle, HCkByteData outBytes);
#endif
CK_VISIBLE_PUBLIC BOOL CkZipW_WriteToMemory(HCkZipW cHandle, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkZipW_WriteZip(HCkZipW cHandle);
CK_VISIBLE_PUBLIC BOOL CkZipW_WriteZipAndClose(HCkZipW cHandle);
#endif
