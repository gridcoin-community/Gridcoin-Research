// CkCsv.h: interface for the CkCsv class.
//
//////////////////////////////////////////////////////////////////////

// This header is generated for Chilkat v9.5.0

#ifndef _CkCsv_H
#define _CkCsv_H
	
#include "chilkatDefs.h"

#include "CkString.h"
#include "CkMultiByteBase.h"




#ifndef __sun__
#pragma pack (push, 8)
#endif
 

// CLASS: CkCsv
class CK_VISIBLE_PUBLIC CkCsv  : public CkMultiByteBase
{
    private:
	

	// Don't allow assignment or copying these objects.
	CkCsv(const CkCsv &);
	CkCsv &operator=(const CkCsv &);

    public:
	CkCsv(void);
	virtual ~CkCsv(void);

	static CkCsv *createNew(void);
	void CK_VISIBLE_PRIVATE inject(void *impl);

	// May be called when finished with the object to free/dispose of any
	// internal resources held by the object. 
	void dispose(void);

	

	// BEGIN PUBLIC INTERFACE

	// ----------------------
	// Properties
	// ----------------------
	// If true, then CRLF line endings are used when saving the CSV to a file or to a
	// string (i.e. for the methods SaveFile, SaveFile2, SaveToString). If false then
	// bare LF line-endings are used.
	bool get_Crlf(void);
	// If true, then CRLF line endings are used when saving the CSV to a file or to a
	// string (i.e. for the methods SaveFile, SaveFile2, SaveToString). If false then
	// bare LF line-endings are used.
	void put_Crlf(bool newVal);

	// The character that separates fields in a record. It is a comma by default. If
	// the Delimiter property is not explicitly set, the CSV component will detect the
	// delimiter when loading a CSV. (Semicolons are typically used in locales where
	// the comma is used as a decimal point.)
	void get_Delimiter(CkString &str);
	// The character that separates fields in a record. It is a comma by default. If
	// the Delimiter property is not explicitly set, the CSV component will detect the
	// delimiter when loading a CSV. (Semicolons are typically used in locales where
	// the comma is used as a decimal point.)
	const char *delimiter(void);
	// The character that separates fields in a record. It is a comma by default. If
	// the Delimiter property is not explicitly set, the CSV component will detect the
	// delimiter when loading a CSV. (Semicolons are typically used in locales where
	// the comma is used as a decimal point.)
	void put_Delimiter(const char *newVal);

	// Set to true prior to loading a CSV if the 1st record contains column names.
	// This allows the CSV parser to correctly load the column names and not treat them
	// as data.
	bool get_HasColumnNames(void);
	// Set to true prior to loading a CSV if the 1st record contains column names.
	// This allows the CSV parser to correctly load the column names and not treat them
	// as data.
	void put_HasColumnNames(bool newVal);

	// The number of columns in the 1st row, which may be the row containing column
	// names if HasColumnNames is true.
	int get_NumColumns(void);

	// The number of data rows. If the CSV has column names, the 1st row is not
	// included in the count. Also, empty lines containing only whitespace characters
	// that follow the last non-empty row are not included.
	int get_NumRows(void);

	// If true, then the strings returned by GetCell and GetCellByName are
	// auto-trimmed of whitespace from both ends.
	bool get_AutoTrim(void);
	// If true, then the strings returned by GetCell and GetCellByName are
	// auto-trimmed of whitespace from both ends.
	void put_AutoTrim(bool newVal);



	// ----------------------
	// Methods
	// ----------------------
	// Deletes the Nth column. (The 1st column is at index 0.)
	bool DeleteColumn(int index);

	// Deletes a column specified by name.
	bool DeleteColumnByName(const char *columnName);

	// Deletes the entire Nth row. (The 1st row is at index 0.)
	bool DeleteRow(int index);

	// Returns the contents of the cell at row,  col. Indexing begins at 0. (The
	// topmost/leftmost cell is at 0,0)
	bool GetCell(int row, int col, CkString &outStr);
	// Returns the contents of the cell at row,  col. Indexing begins at 0. (The
	// topmost/leftmost cell is at 0,0)
	const char *getCell(int row, int col);
	// Returns the contents of the cell at row,  col. Indexing begins at 0. (The
	// topmost/leftmost cell is at 0,0)
	const char *cell(int row, int col);

	// The same as GetCell, but the column is specified by name instead of by index.
	bool GetCellByName(int row, const char *columnName, CkString &outStr);
	// The same as GetCell, but the column is specified by name instead of by index.
	const char *getCellByName(int row, const char *columnName);
	// The same as GetCell, but the column is specified by name instead of by index.
	const char *cellByName(int row, const char *columnName);

	// Returns the name of the Nth column.
	bool GetColumnName(int index, CkString &outStr);
	// Returns the name of the Nth column.
	const char *getColumnName(int index);
	// Returns the name of the Nth column.
	const char *columnName(int index);

	// Returns the column index for a given column.
	int GetIndex(const char *columnName);

	// Returns the number of columns for a specific row. If the row is larger than the
	// number of rows in the CSV, a zero is returned.
	int GetNumCols(int row);

	// Loads a CSV from a file. It is assumed that the CSV file contains ANSI
	// characters.
	bool LoadFile(const char *path);

	// Loads a CSV from a file. The  charset specifies the character encoding of the CSV
	// file. A list of supported character encodings may be found on this page:
	// Supported Charsets
	// <http://www.chilkatsoft.com/p/p_463.asp> .
	bool LoadFile2(const char *path, const char *charset);

	// Loads a CSV document from an in-memory string variable.
	bool LoadFromString(const char *csvData);

	// Compares the contents of an entire row to a wildcarded match pattern where "*"
	// can be used any number of times to match 0 or more of any character. Returns
	// true if a match was found, otherwise returns false. If  caseSensitive is true, then
	// the pattern match is case sensitive, otherwise it is case insensitive.
	bool RowMatches(int row, const char *matchPattern, bool bCaseSensitive);

	// Saves a CSV to a file. The output file is written using the ANSI character
	// encoding.
	bool SaveFile(const char *path);

	// Saves a CSV to a file. The  charset specifies the character encoding to use for the
	// CSV file. The text data is converted to this charset when saving. A list of
	// supported character encodings may be found on this page: Supported Charsets
	// <http://www.chilkatsoft.com/p/p_463.asp> .
	bool SaveFile2(const char *path, const char *charset);

	// Writes the entire CSV document to a string variable.
	bool SaveToString(CkString &outStr);
	// Writes the entire CSV document to a string variable.
	const char *saveToString(void);

	// Sets the contents for a single cell in the CSV. The content may include any
	// characters including CRLF's, double-quotes, and the delimiter character. The
	// Save* methods automatically double-quote fields with special chars when saving.
	// The Load* methods automatically parse double-quoted and/or escaped fields
	// correctly when loading.
	bool SetCell(int row, int col, const char *content);

	// The same as SetCell, except the column is specified by name instead of by index.
	bool SetCellByName(int row, const char *columnName, const char *content);

	// Sets the name of the Nth column. The first column is at index 0. This method
	// would only return false if an invalid index is passed (such as a negative
	// number).
	bool SetColumnName(int index, const char *columnName);

	// Sorts the rows in the CSV by the contents of a specific column. If  ascending is
	// true, the sort is in ascending order, otherwise descending order. If  caseSensitive is
	// true then the sorting is case sensitive.
	bool SortByColumn(const char *columnName, bool bAscending, bool bCaseSensitive);





	// END PUBLIC INTERFACE


};
#ifndef __sun__
#pragma pack (pop)
#endif


	
#endif
