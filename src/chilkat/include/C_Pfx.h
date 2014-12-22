// This is a generated source file for Chilkat version 9.5.0.31
#if defined(WIN32) || defined(WINCE)

#ifndef _C_Pfx_H
#define _C_Pfx_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

HPfx Pfx_Create(void);
void Pfx_Dispose(HPfx handle);
void Pfx_getDebugLogFilePath(HPfx cHandle, HCkString retval);
void Pfx_putDebugLogFilePath(HPfx cHandle, const char *newVal);
const char *Pfx_debugLogFilePath(HPfx cHandle);
void Pfx_getLastErrorHtml(HPfx cHandle, HCkString retval);
const char *Pfx_lastErrorHtml(HPfx cHandle);
void Pfx_getLastErrorText(HPfx cHandle, HCkString retval);
const char *Pfx_lastErrorText(HPfx cHandle);
void Pfx_getLastErrorXml(HPfx cHandle, HCkString retval);
const char *Pfx_lastErrorXml(HPfx cHandle);
BOOL Pfx_getUtf8(HPfx cHandle);
void Pfx_putUtf8(HPfx cHandle, BOOL newVal);
BOOL Pfx_getVerboseLogging(HPfx cHandle);
void Pfx_putVerboseLogging(HPfx cHandle, BOOL newVal);
void Pfx_getVersion(HPfx cHandle, HCkString retval);
const char *Pfx_version(HPfx cHandle);
BOOL Pfx_SaveLastError(HPfx cHandle, const char *path);
#endif

#endif // WIN32 (entire file)
