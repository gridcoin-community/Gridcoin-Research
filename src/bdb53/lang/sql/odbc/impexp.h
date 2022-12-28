#ifndef _IMPEXP_H
#define _IMPEXP_H

/*
 * 2007 January 27
 *
 * The author disclaims copyright to this source code.  In place of
 * a legal notice, here is a blessing:
 *
 *    May you do good and not evil.
 *    May you find forgiveness for yourself and forgive others.
 *    May you share freely, never taking more than you give.
 *
 ********************************************************************
 *
 * SQLite extension module for importing/exporting
 * database information from/to SQL source text and
 * export to CSV text.
 */

/*
 *  impexp_import_sql
 *
 *       Reads SQL commands from filename and executes them
 *       against the current database. Returns the number
 *       of changes to the current database.
 */

int impexp_import_sql(sqlite3 *db, char *filename);

/*
 *  impexp_export_sql
 *
 *       Writes SQL to filename similar to SQLite's shell
 *       ".dump" meta command. Mode selects the output format:
 *       Mode 0 (default): dump schema and data using the
 *       optional table names following the mode argument.
 *       Mode 1: dump data only using the optional table
 *       names following the mode argument.
 *       Mode 2: dump schema and data using the optional
 *       table names following the mode argument; each
 *       table name is followed by a WHERE clause, i.e.
 *       "mode, table1, where1, table2, where2, ..."
 *       Mode 3: dump data only, same rules as in mode 2.
 *       Returns approximate number of lines written or
 *       -1 when an error occurred.
 *
 *       Bit 1 of mode:      when 1 dump data only
 *       Bits 8..9 of mode:  blob quoting mode
 *           0   default
 *         256   ORACLE
 *         512   SQL Server
 *         768   MySQL
 */

int impexp_export_sql(sqlite3 *db, char *filename, int mode, ...);

/*
 *  impexp_export_csv
 *
 *       Writes entire tables as CSV to provided filename. A header
 *       row is written when the hdr parameter is true. The
 *       rows are optionally introduced with a column made up of
 *       the prefix (non-empty string) for the respective table.
 *       If "schema" is NULL, "sqlite_master" is used, otherwise
 *       specify e.g. "sqlite_temp_master" for temporary tables or 
 *       "att.sqlite_master" for the attached database "att".
 *
 *          CREATE TABLE A(a,b);
 *          INSERT INTO A VALUES(1,2);
 *          INSERT INTO A VALUES(3,'foo');
 *          CREATE TABLE B(c);
 *          INSERT INTO B VALUES('hello');
 *          SELECT export_csv('out.csv', 0, 'aa', 'A', NULL, 'bb', 'B', NULL);
 *          -- CSV output
 *          "aa",1,2
 *          "aa",3,"foo"
 *          "bb","hello"
 *          SELECT export_csv('out.csv', 1, 'aa', 'A', NULL, 'bb', 'B', NULL);
 *          -- CSV output
 *          "aa","a","b"
 *          "aa",1,2
 *          "aa",3,"foo"
 *          "bb","c"
 *          "bb","hello"
 */

int impexp_export_csv(sqlite3 *db, char *filename, int hdr, ...);

/*
 *  impexp_export_xml
 *
 *       Writes a table as simple XML to provided filename. The
 *       rows are optionally enclosed with the "root" tag,
 *       the row data is enclosed in "item" tags. If "schema"
 *       is NULL, "sqlite_master" is used, otherwise specify
 *       e.g. "sqlite_temp_master" for temporary tables or 
 *       "att.sqlite_master" for the attached database "att".
 *          
 *          <item>
 *           <columnname TYPE="INTEGER|REAL|NULL|TEXT|BLOB">value</columnname>
 *           ...
 *          </item>
 *
 *       e.g.
 *
 *          CREATE TABLE A(a,b);
 *          INSERT INTO A VALUES(1,2.1);
 *          INSERT INTO A VALUES(3,'foo');
 *          INSERT INTO A VALUES('',NULL);
 *          INSERT INTO A VALUES(X'010203','<blob>');
 *          SELECT export_xml('out.xml', 0, 2, 'TBL_A', 'ROW', 'A');
 *          -- XML output
 *            <TBL_A>
 *             <ROW>
 *              <a TYPE="INTEGER">1</a>
 *              <b TYPE="REAL">2.1</b>
 *             </ROW>
 *             <ROW>
 *              <a TYPE="INTEGER">3</a>
 *              <b TYPE="TEXT">foo</b>
 *             </ROW>
 *             <ROW>
 *              <a TYPE="TEXT"></a>
 *              <b TYPE="NULL"></b>
 *             </ROW>
 *             <ROW>
 *              <a TYPE="BLOB">&#x01;&#x02;&x03;</a>
 *              <b TYPE="TEXT">&lt;blob&gt;</b>
 *             </ROW>
 *            </TBL_A>
 *
 *       Quoting of XML entities is performed only on the data,
 *       not on column names and root/item tags.
 */

int impexp_export_xml(sqlite3 *db, char *filename,
		      int append, int indent, char *root,
		      char *item, char *tablename, char *schema);


/*
 *  impexp_export_json
 *
 *       Executes arbitrary SQL statements and formats
 *       the result in JavaScript Object Notation (JSON).
 *       The layout of the result is:
 *
 *        object {results, sql}
 *         results[] object {columns, rows, changes, last_insert_rowid, error}
 *          columns[]
 *           object {name, decltype, type }     (sqlite3_column_*)
 *          rows[][]                            (sqlite3_column_*)
 *          changes                             (sqlite3_changes)
 *          last_insert_rowid                   (sqlite3_last_insert_rowid)
 *          error                               (sqlite3_errmsg)
 *         sql                                  (SQL text)
 *
 *       For each single SQL statement in "sql" an object in the
 *       "results" array is produced.
 *
 *       The function pointer for the output function to
 *       "impexp_export_json" has a signature compatible
 *       with fputc(3).
 */

typedef void (*impexp_putc)(int c, void *arg);

int impexp_export_json(sqlite3 *db, char *sql, impexp_putc pfunc,
		       void *parg);

/*
 *  impexp_init
 *
 *       Registers the SQLite functions
 *
 *          import_sql(filename)
 *          export_sql(filename, [mode, tablename, ...])
 *          export_csv(filename, hdr, prefix1, tablename1, schema1, ...)
 *          export_xml(filename, appendflg, indent,
 *                     [root, item, tablename, schema]+)
 *          export_json(filename, sql)
 *
 *       On Win32 the filename argument may be specified as NULL in
 *       order to open a system file dialog for interactive filename
 *       selection.
 */
	
int impexp_init(sqlite3 *db);


#endif
