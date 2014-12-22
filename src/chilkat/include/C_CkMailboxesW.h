// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkMailboxesWH
#define _C_CkMailboxesWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkMailboxesW CkMailboxesW_Create(void);
CK_VISIBLE_PUBLIC void CkMailboxesW_Dispose(HCkMailboxesW handle);
CK_VISIBLE_PUBLIC int CkMailboxesW_getCount(HCkMailboxesW cHandle);
CK_VISIBLE_PUBLIC void CkMailboxesW_getDebugLogFilePath(HCkMailboxesW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkMailboxesW_putDebugLogFilePath(HCkMailboxesW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkMailboxesW_debugLogFilePath(HCkMailboxesW cHandle);
CK_VISIBLE_PUBLIC void CkMailboxesW_getLastErrorHtml(HCkMailboxesW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkMailboxesW_lastErrorHtml(HCkMailboxesW cHandle);
CK_VISIBLE_PUBLIC void CkMailboxesW_getLastErrorText(HCkMailboxesW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkMailboxesW_lastErrorText(HCkMailboxesW cHandle);
CK_VISIBLE_PUBLIC void CkMailboxesW_getLastErrorXml(HCkMailboxesW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkMailboxesW_lastErrorXml(HCkMailboxesW cHandle);
CK_VISIBLE_PUBLIC BOOL CkMailboxesW_getVerboseLogging(HCkMailboxesW cHandle);
CK_VISIBLE_PUBLIC void CkMailboxesW_putVerboseLogging(HCkMailboxesW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkMailboxesW_getVersion(HCkMailboxesW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkMailboxesW_version(HCkMailboxesW cHandle);
CK_VISIBLE_PUBLIC BOOL CkMailboxesW_GetFlags(HCkMailboxesW cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkMailboxesW_getFlags(HCkMailboxesW cHandle, int index);
CK_VISIBLE_PUBLIC int CkMailboxesW_GetMailboxIndex(HCkMailboxesW cHandle, const wchar_t *mbxName);
CK_VISIBLE_PUBLIC BOOL CkMailboxesW_GetName(HCkMailboxesW cHandle, int index, HCkString outStrName);
CK_VISIBLE_PUBLIC const wchar_t *CkMailboxesW_getName(HCkMailboxesW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkMailboxesW_GetNthFlag(HCkMailboxesW cHandle, int index, int flagIndex, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkMailboxesW_getNthFlag(HCkMailboxesW cHandle, int index, int flagIndex);
CK_VISIBLE_PUBLIC int CkMailboxesW_GetNumFlags(HCkMailboxesW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkMailboxesW_HasFlag(HCkMailboxesW cHandle, int index, const wchar_t *flagName);
CK_VISIBLE_PUBLIC BOOL CkMailboxesW_HasInferiors(HCkMailboxesW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkMailboxesW_IsMarked(HCkMailboxesW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkMailboxesW_IsSelectable(HCkMailboxesW cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkMailboxesW_SaveLastError(HCkMailboxesW cHandle, const wchar_t *path);
#endif
