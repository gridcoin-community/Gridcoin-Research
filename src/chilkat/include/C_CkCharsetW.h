// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCharsetWH
#define _C_CkCharsetWH
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCharsetW CkCharsetW_Create(void);
CK_VISIBLE_PUBLIC void CkCharsetW_Dispose(HCkCharsetW handle);
CK_VISIBLE_PUBLIC void CkCharsetW_getAltToCharset(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCharsetW_putAltToCharset(HCkCharsetW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_altToCharset(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC void CkCharsetW_getDebugLogFilePath(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCharsetW_putDebugLogFilePath(HCkCharsetW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_debugLogFilePath(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC int CkCharsetW_getErrorAction(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC void CkCharsetW_putErrorAction(HCkCharsetW cHandle, int newVal);
CK_VISIBLE_PUBLIC void CkCharsetW_getFromCharset(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCharsetW_putFromCharset(HCkCharsetW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_fromCharset(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC void CkCharsetW_getLastErrorHtml(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_lastErrorHtml(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC void CkCharsetW_getLastErrorText(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_lastErrorText(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC void CkCharsetW_getLastErrorXml(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_lastErrorXml(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC void CkCharsetW_getLastInputAsHex(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_lastInputAsHex(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC void CkCharsetW_getLastInputAsQP(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_lastInputAsQP(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC void CkCharsetW_getLastOutputAsHex(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_lastOutputAsHex(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC void CkCharsetW_getLastOutputAsQP(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_lastOutputAsQP(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_getSaveLast(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC void CkCharsetW_putSaveLast(HCkCharsetW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCharsetW_getToCharset(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCharsetW_putToCharset(HCkCharsetW cHandle, const wchar_t *newVal);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_toCharset(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_getVerboseLogging(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC void CkCharsetW_putVerboseLogging(HCkCharsetW cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCharsetW_getVersion(HCkCharsetW cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_version(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC int CkCharsetW_CharsetToCodePage(HCkCharsetW cHandle, const wchar_t *charsetName);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_CodePageToCharset(HCkCharsetW cHandle, int codePage, HCkString outCharset);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_codePageToCharset(HCkCharsetW cHandle, int codePage);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_ConvertData(HCkCharsetW cHandle, HCkByteData inData, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_ConvertFile(HCkCharsetW cHandle, const wchar_t *inPath, const wchar_t *outPath);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_ConvertFileNoPreamble(HCkCharsetW cHandle, const wchar_t *inPath, const wchar_t *outPath);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_ConvertFromUnicode(HCkCharsetW cHandle, const wchar_t *inData, HCkByteData outBytes);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_ConvertFromUtf16(HCkCharsetW cHandle, HCkByteData uniData, HCkByteData outMbData);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_ConvertHtml(HCkCharsetW cHandle, HCkByteData inData, HCkByteData outHtml);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_ConvertHtmlFile(HCkCharsetW cHandle, const wchar_t *inPath, const wchar_t *outPath);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_ConvertToUnicode(HCkCharsetW cHandle, HCkByteData inData, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_convertToUnicode(HCkCharsetW cHandle, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_ConvertToUtf16(HCkCharsetW cHandle, HCkByteData mbData, HCkByteData outUniData);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_EntityEncodeDec(HCkCharsetW cHandle, const wchar_t *str, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_entityEncodeDec(HCkCharsetW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_EntityEncodeHex(HCkCharsetW cHandle, const wchar_t *str, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_entityEncodeHex(HCkCharsetW cHandle, const wchar_t *str);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_GetHtmlCharset(HCkCharsetW cHandle, HCkByteData inData, HCkString outCharset);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_getHtmlCharset(HCkCharsetW cHandle, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_GetHtmlFileCharset(HCkCharsetW cHandle, const wchar_t *htmlFilePath, HCkString outCharset);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_getHtmlFileCharset(HCkCharsetW cHandle, const wchar_t *htmlFilePath);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_HtmlDecodeToStr(HCkCharsetW cHandle, const wchar_t *inStr, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_htmlDecodeToStr(HCkCharsetW cHandle, const wchar_t *inStr);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_HtmlEntityDecode(HCkCharsetW cHandle, HCkByteData inHtml, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_HtmlEntityDecodeFile(HCkCharsetW cHandle, const wchar_t *inPath, const wchar_t *outPath);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_IsUnlocked(HCkCharsetW cHandle);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_LowerCase(HCkCharsetW cHandle, const wchar_t *inStr, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_lowerCase(HCkCharsetW cHandle, const wchar_t *inStr);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_ReadFile(HCkCharsetW cHandle, const wchar_t *path, HCkByteData outData);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_ReadFileToString(HCkCharsetW cHandle, const wchar_t *path, const wchar_t *charset, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_readFileToString(HCkCharsetW cHandle, const wchar_t *path, const wchar_t *charset);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_SaveLastError(HCkCharsetW cHandle, const wchar_t *path);
CK_VISIBLE_PUBLIC void CkCharsetW_SetErrorBytes(HCkCharsetW cHandle, HCkByteData data);
CK_VISIBLE_PUBLIC void CkCharsetW_SetErrorString(HCkCharsetW cHandle, const wchar_t *str, const wchar_t *charset);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_UnlockComponent(HCkCharsetW cHandle, const wchar_t *unlockCode);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_UpperCase(HCkCharsetW cHandle, const wchar_t *inStr, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_upperCase(HCkCharsetW cHandle, const wchar_t *inStr);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_UrlDecodeStr(HCkCharsetW cHandle, const wchar_t *inStr, HCkString outStr);
CK_VISIBLE_PUBLIC const wchar_t *CkCharsetW_urlDecodeStr(HCkCharsetW cHandle, const wchar_t *inStr);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_VerifyData(HCkCharsetW cHandle, const wchar_t *charset, HCkByteData inData);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_VerifyFile(HCkCharsetW cHandle, const wchar_t *charset, const wchar_t *path);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_WriteFile(HCkCharsetW cHandle, const wchar_t *path, HCkByteData byteData);
CK_VISIBLE_PUBLIC BOOL CkCharsetW_WriteStringToFile(HCkCharsetW cHandle, const wchar_t *textData, const wchar_t *path, const wchar_t *charset);
#endif
