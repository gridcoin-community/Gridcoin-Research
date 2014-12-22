// This is a generated source file for Chilkat version 9.5.0.31
#if defined(WIN32) || defined(WINCE)

#ifndef _C_PfxWH
#define _C_PfxWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

HPfxW PfxW_Create(void);
void PfxW_Dispose(HPfxW handle);
void PfxW_getDebugLogFilePath(HPfxW cHandle, HCkString retval);
void PfxW_putDebugLogFilePath(HPfxW cHandle, const wchar_t *newVal);
const wchar_t *PfxW_debugLogFilePath(HPfxW cHandle);
void PfxW_getLastErrorHtml(HPfxW cHandle, HCkString retval);
const wchar_t *PfxW_lastErrorHtml(HPfxW cHandle);
void PfxW_getLastErrorText(HPfxW cHandle, HCkString retval);
const wchar_t *PfxW_lastErrorText(HPfxW cHandle);
void PfxW_getLastErrorXml(HPfxW cHandle, HCkString retval);
const wchar_t *PfxW_lastErrorXml(HPfxW cHandle);
BOOL PfxW_getVerboseLogging(HPfxW cHandle);
void PfxW_putVerboseLogging(HPfxW cHandle, BOOL newVal);
void PfxW_getVersion(HPfxW cHandle, HCkString retval);
const wchar_t *PfxW_version(HPfxW cHandle);
BOOL PfxW_SaveLastError(HPfxW cHandle, const wchar_t *path);
#endif

#endif // WIN32 (entire file)
