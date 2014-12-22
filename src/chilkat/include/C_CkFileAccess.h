// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkFileAccess_H
#define _C_CkFileAccess_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkFileAccess CkFileAccess_Create(void);
CK_VISIBLE_PUBLIC void CkFileAccess_Dispose(HCkFileAccess handle);
CK_VISIBLE_PUBLIC void CkFileAccess_getCurrentDir(HCkFileAccess cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkFileAccess_currentDir(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC void CkFileAccess_getDebugLogFilePath(HCkFileAccess cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkFileAccess_putDebugLogFilePath(HCkFileAccess cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkFileAccess_debugLogFilePath(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_getEndOfFile(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC int CkFileAccess_getFileOpenError(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC void CkFileAccess_getFileOpenErrorMsg(HCkFileAccess cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkFileAccess_fileOpenErrorMsg(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC void CkFileAccess_getLastErrorHtml(HCkFileAccess cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkFileAccess_lastErrorHtml(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC void CkFileAccess_getLastErrorText(HCkFileAccess cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkFileAccess_lastErrorText(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC void CkFileAccess_getLastErrorXml(HCkFileAccess cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkFileAccess_lastErrorXml(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_getUtf8(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC void CkFileAccess_putUtf8(HCkFileAccess cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_getVerboseLogging(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC void CkFileAccess_putVerboseLogging(HCkFileAccess cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkFileAccess_getVersion(HCkFileAccess cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkFileAccess_version(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_AppendAnsi(HCkFileAccess cHandle, const char *text);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_AppendText(HCkFileAccess cHandle, const char *str, const char *charset);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_AppendUnicodeBOM(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_AppendUtf8BOM(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_DirAutoCreate(HCkFileAccess cHandle, const char *dirPath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_DirCreate(HCkFileAccess cHandle, const char *dirPath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_DirDelete(HCkFileAccess cHandle, const char *dirPath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_DirEnsureExists(HCkFileAccess cHandle, const char *filePath);
CK_VISIBLE_PUBLIC void CkFileAccess_FileClose(HCkFileAccess cHandle);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_FileContentsEqual(HCkFileAccess cHandle, const char *filePath1, const char *filePath2);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_FileCopy(HCkFileAccess cHandle, const char *existingFilepath, const char *newFilepath, BOOL failIfExists);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_FileDelete(HCkFileAccess cHandle, const char *filePath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_FileExists(HCkFileAccess cHandle, const char *filePath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_FileOpen(HCkFileAccess cHandle, const char *filePath, unsigned long accessMode, unsigned long shareMode, unsigned long createDisposition, unsigned long attributes);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_FileRead(HCkFileAccess cHandle, int maxNumBytes, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_FileRename(HCkFileAccess cHandle, const char *existingFilepath, const char *newFilepath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_FileSeek(HCkFileAccess cHandle, int offset, int origin);
CK_VISIBLE_PUBLIC int CkFileAccess_FileSize(HCkFileAccess cHandle, const char *filePath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_FileWrite(HCkFileAccess cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_GetTempFilename(HCkFileAccess cHandle, const char *dirPath, const char *prefix, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkFileAccess_getTempFilename(HCkFileAccess cHandle, const char *dirPath, const char *prefix);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_OpenForAppend(HCkFileAccess cHandle, const char *filePath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_OpenForRead(HCkFileAccess cHandle, const char *filePath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_OpenForReadWrite(HCkFileAccess cHandle, const char *filePath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_OpenForWrite(HCkFileAccess cHandle, const char *filePath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_ReadBinaryToEncoded(HCkFileAccess cHandle, const char *filePath, const char *encoding, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkFileAccess_readBinaryToEncoded(HCkFileAccess cHandle, const char *filePath, const char *encoding);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_ReadEntireFile(HCkFileAccess cHandle, const char *filePath, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_ReadEntireTextFile(HCkFileAccess cHandle, const char *filePath, const char *charset, HCkString outStrFileContents);
CK_VISIBLE_PUBLIC const char *CkFileAccess_readEntireTextFile(HCkFileAccess cHandle, const char *filePath, const char *charset);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_ReassembleFile(HCkFileAccess cHandle, const char *partsDirPath, const char *partPrefix, const char *partExtension, const char *reassembledFilename);
CK_VISIBLE_PUBLIC int CkFileAccess_ReplaceStrings(HCkFileAccess cHandle, const char *filePath, const char *charset, const char *existingString, const char *replacementString);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_SaveLastError(HCkFileAccess cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_SetCurrentDir(HCkFileAccess cHandle, const char *dirPath);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_SetFileTimes(HCkFileAccess cHandle, const char *filePath, HCkDateTime createTime, HCkDateTime lastAccessTime, HCkDateTime lastModTime);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_SetLastModified(HCkFileAccess cHandle, const char *filePath, HCkDateTime lastModified);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_SplitFile(HCkFileAccess cHandle, const char *fileToSplit, const char *partPrefix, const char *partExtension, int partSize, const char *destDir);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_TreeDelete(HCkFileAccess cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_WriteEntireFile(HCkFileAccess cHandle, const char *filePath, HCkByteData fileData);
CK_VISIBLE_PUBLIC BOOL CkFileAccess_WriteEntireTextFile(HCkFileAccess cHandle, const char *filePath, const char *textData, const char *charset, BOOL includedPreamble);
#endif
