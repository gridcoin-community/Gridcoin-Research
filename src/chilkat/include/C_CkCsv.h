// This is a generated source file for Chilkat version 9.5.0.40
#ifndef _C_CkCsv_H
#define _C_CkCsv_H
#include "chilkatDefs.h"

#include "Chilkat_C.h"

CK_VISIBLE_PUBLIC HCkCsv CkCsv_Create(void);
CK_VISIBLE_PUBLIC void CkCsv_Dispose(HCkCsv handle);
CK_VISIBLE_PUBLIC BOOL CkCsv_getAutoTrim(HCkCsv cHandle);
CK_VISIBLE_PUBLIC void CkCsv_putAutoTrim(HCkCsv cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkCsv_getCrlf(HCkCsv cHandle);
CK_VISIBLE_PUBLIC void CkCsv_putCrlf(HCkCsv cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCsv_getDebugLogFilePath(HCkCsv cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCsv_putDebugLogFilePath(HCkCsv cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCsv_debugLogFilePath(HCkCsv cHandle);
CK_VISIBLE_PUBLIC void CkCsv_getDelimiter(HCkCsv cHandle, HCkString retval);
CK_VISIBLE_PUBLIC void CkCsv_putDelimiter(HCkCsv cHandle, const char *newVal);
CK_VISIBLE_PUBLIC const char *CkCsv_delimiter(HCkCsv cHandle);
CK_VISIBLE_PUBLIC BOOL CkCsv_getHasColumnNames(HCkCsv cHandle);
CK_VISIBLE_PUBLIC void CkCsv_putHasColumnNames(HCkCsv cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCsv_getLastErrorHtml(HCkCsv cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCsv_lastErrorHtml(HCkCsv cHandle);
CK_VISIBLE_PUBLIC void CkCsv_getLastErrorText(HCkCsv cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCsv_lastErrorText(HCkCsv cHandle);
CK_VISIBLE_PUBLIC void CkCsv_getLastErrorXml(HCkCsv cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCsv_lastErrorXml(HCkCsv cHandle);
CK_VISIBLE_PUBLIC int CkCsv_getNumColumns(HCkCsv cHandle);
CK_VISIBLE_PUBLIC int CkCsv_getNumRows(HCkCsv cHandle);
CK_VISIBLE_PUBLIC BOOL CkCsv_getUtf8(HCkCsv cHandle);
CK_VISIBLE_PUBLIC void CkCsv_putUtf8(HCkCsv cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC BOOL CkCsv_getVerboseLogging(HCkCsv cHandle);
CK_VISIBLE_PUBLIC void CkCsv_putVerboseLogging(HCkCsv cHandle, BOOL newVal);
CK_VISIBLE_PUBLIC void CkCsv_getVersion(HCkCsv cHandle, HCkString retval);
CK_VISIBLE_PUBLIC const char *CkCsv_version(HCkCsv cHandle);
CK_VISIBLE_PUBLIC BOOL CkCsv_DeleteColumn(HCkCsv cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCsv_DeleteColumnByName(HCkCsv cHandle, const char *columnName);
CK_VISIBLE_PUBLIC BOOL CkCsv_DeleteRow(HCkCsv cHandle, int index);
CK_VISIBLE_PUBLIC BOOL CkCsv_GetCell(HCkCsv cHandle, int row, int col, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCsv_getCell(HCkCsv cHandle, int row, int col);
CK_VISIBLE_PUBLIC BOOL CkCsv_GetCellByName(HCkCsv cHandle, int rowIndex, const char *columnName, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCsv_getCellByName(HCkCsv cHandle, int rowIndex, const char *columnName);
CK_VISIBLE_PUBLIC BOOL CkCsv_GetColumnName(HCkCsv cHandle, int index, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCsv_getColumnName(HCkCsv cHandle, int index);
CK_VISIBLE_PUBLIC int CkCsv_GetIndex(HCkCsv cHandle, const char *columnName);
CK_VISIBLE_PUBLIC int CkCsv_GetNumCols(HCkCsv cHandle, int row);
CK_VISIBLE_PUBLIC BOOL CkCsv_LoadFile(HCkCsv cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCsv_LoadFile2(HCkCsv cHandle, const char *filename, const char *charset);
CK_VISIBLE_PUBLIC BOOL CkCsv_LoadFromString(HCkCsv cHandle, const char *csvData);
CK_VISIBLE_PUBLIC BOOL CkCsv_RowMatches(HCkCsv cHandle, int rowIndex, const char *matchPattern, BOOL caseSensitive);
CK_VISIBLE_PUBLIC BOOL CkCsv_SaveFile(HCkCsv cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCsv_SaveFile2(HCkCsv cHandle, const char *filename, const char *charset);
CK_VISIBLE_PUBLIC BOOL CkCsv_SaveLastError(HCkCsv cHandle, const char *path);
CK_VISIBLE_PUBLIC BOOL CkCsv_SaveToString(HCkCsv cHandle, HCkString outStr);
CK_VISIBLE_PUBLIC const char *CkCsv_saveToString(HCkCsv cHandle);
CK_VISIBLE_PUBLIC BOOL CkCsv_SetCell(HCkCsv cHandle, int row, int col, const char *content);
CK_VISIBLE_PUBLIC BOOL CkCsv_SetCellByName(HCkCsv cHandle, int rowIndex, const char *columnName, const char *contentStr);
CK_VISIBLE_PUBLIC BOOL CkCsv_SetColumnName(HCkCsv cHandle, int index, const char *columnName);
CK_VISIBLE_PUBLIC BOOL CkCsv_SortByColumn(HCkCsv cHandle, const char *columnName, BOOL ascending, BOOL caseSensitive);
#endif
