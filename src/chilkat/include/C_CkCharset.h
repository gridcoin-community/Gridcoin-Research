// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCharset_H
#define _C_CkCharset_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCharset CkCharset_Create(void);
CK_VISIBLE_PUBLIC void CkCharset_Dispose(HCkCharset handle);
CK_VISIBLE_PUBLIC void CkCharset_getAltToCharset(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCharset_putAltToCharset(HCkCharset cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCharset_altToCharset(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_getDebugLogFilePath(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCharset_putDebugLogFilePath(HCkCharset cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCharset_debugLogFilePath(HCkCharset cHandle);
CK_VISIBLE_PUBLIC int CkCharset_getErrorAction(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_putErrorAction(HCkCharset cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkCharset_getFromCharset(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCharset_putFromCharset(HCkCharset cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCharset_fromCharset(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_getLastErrorHtml(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCharset_lastErrorHtml(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_getLastErrorText(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCharset_lastErrorText(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_getLastErrorXml(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCharset_lastErrorXml(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_getLastInputAsHex(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCharset_lastInputAsHex(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_getLastInputAsQP(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCharset_lastInputAsQP(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_getLastOutputAsHex(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCharset_lastOutputAsHex(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_getLastOutputAsQP(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCharset_lastOutputAsQP(HCkCharset cHandle);
CK_VISIBLE_PUBLIC BOOL CkCharset_getSaveLast(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_putSaveLast(HCkCharset cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCharset_getToCharset(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCharset_putToCharset(HCkCharset cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCharset_toCharset(HCkCharset cHandle);
CK_VISIBLE_PUBLIC BOOL CkCharset_getUtf8(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_putUtf8(HCkCharset cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkCharset_getVerboseLogging(HCkCharset cHandle);
CK_VISIBLE_PUBLIC void CkCharset_putVerboseLogging(HCkCharset cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCharset_getVersion(HCkCharset cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCharset_version(HCkCharset cHandle);
CK_VISIBLE_PUBLIC int CkCharset_CharsetToCodePage(HCkCharset cHandle, const char *charsetName);
CK_VISIBLE_PUBLIC BOOL CkCharset_CodePageToCharset(HCkCharset cHandle, int codePage, HCkString outCharset);
CK_VISIBLE_PUBLIC const char *CkCharset_codePageToCharset(HCkCharset cHandle, int codePage);
CK_VISIBLE_PUBLIC BOOL CkCharset_ConvertData(HCkCharset cHandle, HCkByteData inData, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCharset_ConvertFile(HCkCharset cHandle, const char *inPath, const char *outPath);
CK_VISIBLE_PUBLIC BOOL CkCharset_ConvertFileNoPreamble(HCkCharset cHandle, const char *inPath, const char *outPath);
CK_VISIBLE_PUBLIC BOOL CkCharset_ConvertFromUnicode(HCkCharset cHandle, const char *inData, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkCharset_ConvertFromUtf16(HCkCharset cHandle, HCkByteData uniData, HCkByteData outMbData);
CK_VISIBLE_PUBLIC BOOL CkCharset_ConvertHtml(HCkCharset cHandle, HCkByteData inData, HCkByteData outHtml);
CK_VISIBLE_PUBLIC BOOL CkCharset_ConvertHtmlFile(HCkCharset cHandle, const char *inPath, const char *outPath);
CK_VISIBLE_PUBLIC BOOL CkCharset_ConvertToUnicode(HCkCharset cHandle, HCkByteData inData, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCharset_convertToUnicode(HCkCharset cHandle, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkCharset_ConvertToUtf16(HCkCharset cHandle, HCkByteData mbData, HCkByteData outUniData);
CK_VISIBLE_PUBLIC BOOL CkCharset_EntityEncodeDec(HCkCharset cHandle, const char *str, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCharset_entityEncodeDec(HCkCharset cHandle, const char *str);
CK_VISIBLE_PUBLIC BOOL CkCharset_EntityEncodeHex(HCkCharset cHandle, const char *str, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCharset_entityEncodeHex(HCkCharset cHandle, const char *str);
CK_VISIBLE_PUBLIC BOOL CkCharset_GetHtmlCharset(HCkCharset cHandle, HCkByteData inData, HCkString outCharset);
CK_VISIBLE_PUBLIC const char *CkCharset_getHtmlCharset(HCkCharset cHandle, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkCharset_GetHtmlFileCharset(HCkCharset cHandle, const char *htmlFilePath, HCkString outCharset);
CK_VISIBLE_PUBLIC const char *CkCharset_getHtmlFileCharset(HCkCharset cHandle, const char *htmlFilePath);
CK_VISIBLE_PUBLIC BOOL CkCharset_HtmlDecodeToStr(HCkCharset cHandle, const char *inStr, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCharset_htmlDecodeToStr(HCkCharset cHandle, const char *inStr);
CK_VISIBLE_PUBLIC BOOL CkCharset_HtmlEntityDecode(HCkCharset cHandle, HCkByteData inHtml, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCharset_HtmlEntityDecodeFile(HCkCharset cHandle, const char *inPath, const char *outPath);
CK_VISIBLE_PUBLIC BOOL CkCharset_IsUnlocked(HCkCharset cHandle);
CK_VISIBLE_PUBLIC BOOL CkCharset_LowerCase(HCkCharset cHandle, const char *inStr, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCharset_lowerCase(HCkCharset cHandle, const char *inStr);
CK_VISIBLE_PUBLIC BOOL CkCharset_ReadFile(HCkCharset cHandle, const char *path, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCharset_ReadFileToString(HCkCharset cHandle, const char *path, const char *charset, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCharset_readFileToString(HCkCharset cHandle, const char *path, const char *charset);
CK_VISIBLE_PUBLIC BOOL CkCharset_SaveLastError(HCkCharset cHandle, const char *path);
CK_VISIBLE_PUBLIC void CkCharset_SetErrorBytes(HCkCharset cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC void CkCharset_SetErrorString(HCkCharset cHandle, const char *str, const char *charset);
CK_VISIBLE_PUBLIC BOOL CkCharset_UnlockComponent(HCkCharset cHandle, const char *unlockCode);
CK_VISIBLE_PUBLIC BOOL CkCharset_UpperCase(HCkCharset cHandle, const char *inStr, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCharset_upperCase(HCkCharset cHandle, const char *inStr);
CK_VISIBLE_PUBLIC BOOL CkCharset_UrlDecodeStr(HCkCharset cHandle, const char *inStr, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCharset_urlDecodeStr(HCkCharset cHandle, const char *inStr);
CK_VISIBLE_PUBLIC BOOL CkCharset_VerifyData(HCkCharset cHandle, const char *charset, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkCharset_VerifyFile(HCkCharset cHandle, const char *charset, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCharset_WriteFile(HCkCharset cHandle, const char *path, HCkByteData byteData);
CK_VISIBLE_PUBLIC BOOL CkCharset_WriteStringToFile(HCkCharset cHandle, const char *textData, const char *path, const char *charset);
#endif
