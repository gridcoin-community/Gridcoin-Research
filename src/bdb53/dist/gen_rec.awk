#!/bin/sh -
#
# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 2013 Oracle and/or its affiliates.  All rights reserved.
#
# $Id$
#

# This awk script generates all the log, print, and read routines for the DB
# logging. It also generates a template for the recovery functions (these
# functions must still be edited, but are highly stylized and the initial
# template gets you a fair way along the path).
#
# For a given file prefix.src, we generate a file prefix_auto.c, and a file
# prefix_auto.h that contains:
#
#	external declarations for the file's functions
# 	defines for the physical record types
#	    (logical types are defined in each subsystem manually)
#	structures to contain the data unmarshalled from the log.
#
# This awk script requires that four variables be set when it is called:
#
#	source_file	-- the C source file being created
#	header_file	-- the C #include file being created
#	template_file	-- the template file being created
#
# And stdin must be the input file that defines the recovery setup.
#
# Within each file prefix.src, we use a number of public keywords (documented
# in the reference guide) as well as the following ones which are private to
# DB:
# 	DBPRIVATE	Indicates that a file will be built as part of DB,
#			rather than compiled independently, and so can use
#			DB-private interfaces (such as DB_LOG_NOCOPY).
#	DB		A DB handle.  Logs the dbreg fileid for that handle,
#			and makes the *_log interface take a DB * instead of a
#			DB_ENV *.
#	OP		The low byte contains the page type of the data
#			that needs byte swapping.  The rest is log record
#			specific.
#	PGDBT,PGDDBT	Just like DBT, only we know it stores a page or page
#			header, so we can byte-swap it (once we write the
#			byte-swapping code, which doesn't exist yet).
#	HDR,DATA	Just like DBT, but we know that these contain database
#			records that may need byte-swapping.
#	LOCKS,PGLIST	Just like DBT, but uses a print function for locks or
#			page lists.

BEGIN {
	if (source_file == "" ||
	    header_file == "" || template_file == "") {
	    print "Usage: gen_rec.awk requires three variables to be set:"
	    print "\theader_file\t-- the recover #include file being created"
	    print "\tprint_file\t-- the print source file being created"
	    print "\tsource_file\t-- the recover source file being created"
	    print "\ttemplate_file\t-- the template file being created"
	    exit
	}
	FS="[\t ][\t ]*"
	CFILE=source_file
	HFILE=header_file
	PFILE=print_file
	TFILE=template_file

	# These are the variables we use to create code that calls into
	# db routines and/or uses an environment.
	dbprivate = 0
	env_type = "DB_ENV"
	env_var = "dbenv"
	log_call = "dbenv->log_put_record"

}
/^[ 	]*DBPRIVATE/ {
	dbprivate = 1
	env_type = "ENV"
	env_var = "env"
	log_call = "__log_put_record"
}
/^[ 	]*PREFIX/ {
	prefix = $2
	num_funcs = 0;

	# Start .c files.
	printf("/* Do not edit: automatically built by gen_rec.awk. */\n\n")\
	    > CFILE
	printf("#include \"db_config.h\"\n") >> CFILE
	if (!dbprivate) {
		printf("#include <errno.h>\n") >> CFILE
		printf("#include <stdlib.h>\n") >> CFILE
		printf("#include <string.h>\n") >> CFILE
		printf("#include \"db_int.h\"\n") >> CFILE
		printf("#include \"dbinc/db_swap.h\"\n") >> CFILE
	}

	printf("/* Do not edit: automatically built by gen_rec.awk. */\n\n")\
	    > PFILE
	printf("#include \"db_config.h\"\n\n") >> PFILE
	if (!dbprivate) {
		printf("#include <ctype.h>\n") >> PFILE
		printf("#include <stdio.h>\n") >> PFILE
		printf("#include <stdlib.h>\n") >> PFILE
		printf("#include \"db_int.h\"\n") >> PFILE
		printf("#include \"dbinc/log.h\"\n") >> PFILE
	}

	if (prefix == "__ham")
		printf("#ifdef HAVE_HASH\n") >> PFILE
	if (prefix == "__heap")
		printf("#ifdef HAVE_HEAP\n") >> PFILE
	if (prefix == "__qam")
		printf("#ifdef HAVE_QUEUE\n") >> PFILE
	if (prefix == "__repmgr")
		printf("#ifdef HAVE_REPLICATION_THREADS\n") >> PFILE

	# Start .h file, make the entire file conditional.
	printf("/* Do not edit: automatically built by gen_rec.awk. */\n\n")\
	    > HFILE
	printf("#ifndef\t%s_AUTO_H\n#define\t%s_AUTO_H\n", prefix, prefix)\
	    >> HFILE
	if (prefix == "__ham")
		printf("#ifdef HAVE_HASH\n") >> HFILE
	if (prefix == "__heap")
		printf("#ifdef HAVE_HEAP\n") >> HFILE
	if (prefix == "__qam")
		printf("#ifdef HAVE_QUEUE\n") >> HFILE
	if (prefix == "__repmgr")
		printf("#ifdef HAVE_REPLICATION_THREADS\n") >> HFILE
	if (dbprivate)
		printf("#include \"dbinc/log.h\"\n") >> HFILE

	# Write recovery template file headers
	if (dbprivate) {
		# This assumes we're doing DB recovery.
		printf("#include \"db_config.h\"\n\n") > TFILE
		printf("#include \"db_int.h\"\n") >> TFILE
		printf("#include \"dbinc/db_page.h\"\n") >> TFILE
		printf("#include \"dbinc/%s.h\"\n", prefix) >> TFILE
		printf("#include \"dbinc/log.h\"\n\n") >> TFILE
	} else {
		printf("#include \"db.h\"\n\n") > TFILE
	}
}
/^[ 	]*INCLUDE/ {
	for (i = 2; i < NF; i++)
		printf("%s ", $i) >> CFILE
	printf("%s\n", $i) >> CFILE
	for (i = 2; i < NF; i++)
		printf("%s ", $i) >> PFILE
	printf("%s\n", $i) >> PFILE
}
/^[ 	]*(BEGIN|BEGIN_COMPAT)/ {
	if (in_begin) {
		print "Invalid format: missing END statement"
		exit
	}
	in_begin = 1;
	is_duplicate = 0;
	is_dbt = 0;
	has_dbp = 0;
	has_data = 0;
	is_uint = 0;
	hdrdbt = "NULL";
	ddbt = "NULL";
	#
	# BEGIN_COMPAT does not need logging function or rec table entry.
	#
	need_log_function = ($1 == "BEGIN");
	is_compat = ($1 == "BEGIN_COMPAT");
	nvars = 0;

	thisfunc = $2;
	dup_thisfunc = $2;
	version = $3;

	rectype = $4;

	make_name(thisfunc, thisfunc, version);
}
/^[	 ]*(DB|ARG|DBOP|DBT|DATA|HDR|LOCKS|OP|PGDBT|PGDDBT|PGLIST|POINTER|TIME)/ {
	vars[nvars] = $2;
	types[nvars] = $3;
	modes[nvars] = $1;
	formats[nvars] = $NF;
	for (i = 4; i < NF; i++)
		types[nvars] = sprintf("%s %s", types[nvars], $i);

	if ($1 == "DB") {
		has_dbp = 1;
	}
	if ($1 == "DATA" || $1 == "PGDDBT") {
		if (has_data == 1) {
			print "Invalid format: multiple data fields"
			exit
		}
		has_data = 1;
	}

	if ($1 == "DB" || $1 == "DBOP" || $1 == "ARG" || \
	    $1 == "OP" || $1 == "TIME") {
		sizes[nvars] = sprintf("sizeof(u_int32_t)");
		if ($3 != "u_int32_t")
			is_uint = 1;
	} else if ($1 == "POINTER")
		sizes[nvars] = sprintf("sizeof(*%s)", $2);
	else { # DBT, PGDBT, PGDDBT
		sizes[nvars] = sprintf("LOG_DBT_SIZE(%s)", $2);
		is_dbt = 1;
		if ($1 == "PGDBT") {
			if (hdrdbt != "NULL") {
				print "Multiple PGDBTs in record"
				exit;
			}
			if (ddbt != "NULL") {
				print "PGDDBT must follow PGDBT"
				exit;
			}
			hdrdbt = vars[nvars];
		} else if ($1 == "PGDDBT") {
			if (ddbt != "NULL") {
				print "Multiple PGDDBTs in record"
				exit;
			}
			if (hdrdbt == "NULL") {
				print "PGDDBT must follow PGDBT"
				exit;
			}
			ddbt = vars[nvars];
		}
	}
	nvars++;
}
/^[	 ]*DUPLICATE/ {
	is_duplicate = 1;
	dup_rectype = $4;
	old_logfunc = logfunc;
	old_funcname = funcname;
	make_name($2, funcname, $3);
	internal_name = sprintf("%s_%s_int", prefix, thisfunc);
	dup_logfunc = logfunc;
	dup_funcname = funcname;
	dup_thisfunc = $2;
	logfunc = old_logfunc;
	funcname = old_funcname;
}
/^[	 ]*END/ {
	if (!in_begin) {
		print "Invalid format: missing BEGIN statement"
		exit;
	}

	# Declare the record type.
	printf("#define\tDB_%s\t%d\n", funcname, rectype) >> HFILE
	if (is_duplicate)
		printf("#define\tDB_%s\t%d\n",\
		    dup_funcname, dup_rectype) >> HFILE

	# Structure declaration.
	printf("typedef struct _%s_args {\n", funcname) >> HFILE

	# Here are the required fields for every structure
	printf("\tu_int32_t type;\n\tDB_TXN *txnp;\n") >> HFILE
	printf("\tDB_LSN prev_lsn;\n") >>HFILE

	# Here are the specified fields.
	for (i = 0; i < nvars; i++) {
		t = types[i];
		if (modes[i] == "POINTER") {
			ndx = index(t, "*");
			t = substr(types[i], 1, ndx - 2);
		}
		printf("\t%s\t%s;\n", t, vars[i]) >> HFILE
	}
	printf("} %s_args;\n\n", funcname) >> HFILE

	# Output the read, log, and print functions (note that we must
	# generate the required read function first, because we use its
	# prototype in the print function).

	log_function(funcname, funcname);
	read_function(funcname, funcname);
	if (is_duplicate) {
		log_function(dup_funcname, funcname);
		read_function(dup_funcname, funcname);
	}
	print_function();

	# Recovery template
	if (dbprivate)
		f = "template/rec_ctemp"
	else
		f = "template/rec_utemp"

	cmd = sprintf(\
    "sed -e s/PREF/%s/ -e s/FUNC/%s/ -e s/DUP/%s/ < template/rec_%s >> %s",
	    prefix, thisfunc, dup_thisfunc,
	    dbprivate ? "ctemp" : "utemp", TFILE)
	system(cmd);

	# Done writing stuff, reset and continue.
	in_begin = 0;
}

END {
	# End the conditional for the HFILE
	if (prefix == "__ham")
		printf("#endif /* HAVE_HASH */\n") >> HFILE
	if (prefix == "__heap")
		printf("#endif /* HAVE_HEAP */\n") >> HFILE
	if (prefix == "__qam")
		printf("#endif /* HAVE_QUEUE */\n") >> HFILE
	if (prefix == "__repmgr")
		printf("#endif /* HAVE_REPLICATION_THREADS */\n") >> HFILE
	printf("#endif\n") >> HFILE

	# Print initialization routine; function prototype
	p[1] = sprintf("int %s_init_print %s%s%s", prefix,
	    "__P((", env_type, " *, DB_DISTAB *));");
	p[2] = "";
	proto_format(p, PFILE);

	# Create the routine to call __db_add_recovery(print_fn, id)
	printf("int\n%s_init_print(%s, dtabp)\n",\
	    prefix, env_var) >> PFILE
	printf("\t%s *%s;\n", env_type, env_var) >> PFILE
	printf("\tDB_DISTAB *dtabp;\n{\n") >> PFILE
	# If application-specific, the user will need a prototype for
	# __db_add_recovery, since they won't have DB's.
	if (!dbprivate) {
		printf(\
		    "\tint __db_add_recovery __P((%s *, DB_DISTAB *,\n",\
		    env_type) >> PFILE
		printf(\
	"\t    int (*)(%s *, DBT *, DB_LSN *, db_recops), u_int32_t));\n",\
		    env_type) >> PFILE
	}

	printf("\tint ret;\n\n") >> PFILE
	for (i = 0; i < num_funcs; i++) {
		if (functable[i] == 1)
			continue;
		printf("\tif ((ret = __db_add_recovery%s(%s, ",\
		    dbprivate ? "_int" : "", env_var) >> PFILE
		printf("dtabp,\n") >> PFILE
		printf("\t    %s_print, DB_%s)) != 0)\n",\
		    dupfuncs[i], funcs[i]) >> PFILE
		printf("\t\treturn (ret);\n") >> PFILE
	}
	printf("\treturn (0);\n}\n") >> PFILE
	if (prefix == "__ham")
		printf("#endif /* HAVE_HASH */\n") >> PFILE
	if (prefix == "__heap")
		printf("#endif /* HAVE_HEAP */\n") >> PFILE
	if (prefix == "__qam")
		printf("#endif /* HAVE_QUEUE */\n") >> PFILE
	if (prefix == "__repmgr")
		printf("#endif /* HAVE_REPLICATION_THREADS */\n") >> PFILE

	# We only want to generate *_init_recover functions if this is a
	# DB-private, rather than application-specific, set of recovery
	# functions.  Application-specific recovery functions should be
	# dispatched using the DB_ENV->set_app_dispatch callback rather than
	# a DB dispatch table ("dtab").
	if (!dbprivate)
		exit
	# Everything below here is dbprivate, so it uses ENV instead of DB_ENV
	# Recover initialization routine
	p[1] = sprintf("int %s_init_recover %s", prefix,\
	    "__P((ENV *, DB_DISTAB *));");
	p[2] = "";
	proto_format(p, CFILE);

	# Create the routine to call db_add_recovery(func, id)
	printf("int\n%s_init_recover(env, dtabp)\n", prefix) >> CFILE
	printf("\tENV *env;\n") >> CFILE
	printf("\tDB_DISTAB *dtabp;\n{\n") >> CFILE
	printf("\tint ret;\n\n") >> CFILE
	for (i = 0; i < num_funcs; i++) {
		if (functable[i] == 1)
			continue;
		printf("\tif ((ret = __db_add_recovery_int(env, ") >> CFILE
		printf("dtabp,\n") >> CFILE
		printf("\t    %s_recover, DB_%s)) != 0)\n",\
		    funcs[i], funcs[i]) >> CFILE
		printf("\t\treturn (ret);\n") >> CFILE
	}
	printf("\treturn (0);\n}\n") >> CFILE
}

function log_function(logfunc, arg)
{
	# Descriptor array
	printf("extern __DB_IMPORT DB_LOG_RECSPEC %s_desc[];\n", \
	    logfunc) >> HFILE;
	printf("DB_LOG_RECSPEC %s_desc[] = {\n", logfunc) >> CFILE

	# Function declaration
	if (need_log_function) {
		printf("static inline int\n%s_log(", logfunc) >> HFILE
		# Now print the parameters
		if (has_dbp) {
			printf("DB *dbp, ") >> HFILE
		} else {
			printf("%s *%s, ", env_type, env_var) >> HFILE
		}
		printf("DB_TXN *txnp, DB_LSN *ret_lsnp, ") >> HFILE
		printf("u_int32_t flags") >> HFILE
	}

	for (i = 0; i < nvars; i++) {
		# Descriptor element
		if (modes[i] == "ARG" || modes[i] == "OP") 
			printf(\
		"\t{LOGREC_%s, SSZ(%s_args, %s), \"%s\", \"%%%s\"},\n",\
			    modes[i], arg, vars[i], \
			    vars[i], formats[i]) >> CFILE
		else
			printf( \
			    "\t{LOGREC_%s, SSZ(%s_args, %s), \"%s\", \"\"},\n",\
			    modes[i], arg, vars[i], vars[i]) >> CFILE

		# Function argument
		# We just skip for modes == DB.
		if (!need_log_function || modes[i] == "DB") 
			continue;
		printf(",") >> HFILE
		if ((i % 5) == 0)
			printf("\n    ") >> HFILE
		else
			printf(" ") >> HFILE
		if (modes[i] == "DBT" || modes[i] == "HDR" ||
		    modes[i] == "DATA" || modes[i] == "LOCKS" ||
		    modes[i] == "PGDBT" || modes[i] == "PGDDBT"||
		    modes[i] == "PGLIST")
			printf("const %s *%s", types[i], vars[i]) >> HFILE
		else
			printf("%s %s", types[i], vars[i]) >> HFILE
	}

	# Descriptor termination
	printf("\t{LOGREC_Done, 0, \"\", \"\"}\n};\n") >> CFILE
	if (!need_log_function)
		return;

	# Function call
	printf(")\n{\n\treturn (%s(", log_call) >> HFILE
	if (dbprivate) {
		if (has_dbp)
			printf("(dbp)->env, dbp, ") >> HFILE
		else
			printf("env, NULL, ") >> HFILE
	} else {
		if (has_dbp)
			printf("dbenv, dbp, ") >> HFILE
		else
			printf("dbenv, NULL, ") >> HFILE
	}
	printf("txnp, ret_lsnp,\n\t    flags, DB_%s, %d,\n\t",
	       logfunc, has_data) >> HFILE
	printf("    sizeof(u_int32_t) + sizeof(u_int32_t) + sizeof(DB_LSN)")\
	    >> HFILE
	for (i = 0; i < nvars; i++) {
		if (i % 3 == 0)
			printf(" +\n\t    %s", sizes[i]) >> HFILE
		else
			printf(" + %s", sizes[i]) >> HFILE
	}
	printf(",\n\t    %s_desc", logfunc) >> HFILE
	for (i = 0; i < nvars; i++) {
		# We just skip for modes == DB.
		if (modes[i] == "DB") 
			continue;
		printf(",") >> HFILE
		if ((i % 8) == 0)
			printf("\n\t    ") >> HFILE
		else
			printf(" ") >> HFILE
		printf("%s", vars[i]) >> HFILE
	}
	printf("));\n}\n\n") >> HFILE
}

function read_function(logfunc, arg)
{
	# Read function
	printf("static inline int %s_read(%s *%s, \n    ", \
	     logfunc, env_type, env_var) >> HFILE
	if (has_dbp)
		printf("DB **dbpp, void *td, ") >> HFILE
	printf("void *data, ") >> HFILE
	printf("%s_args **arg)\n{\n", arg) >> HFILE
	if (dbprivate) {
		printf("\t*arg = NULL;\n") >> HFILE
		printf("\treturn (__log_read_record(env, \n") >> HFILE
		if (has_dbp)
			printf("\t    dbpp, td, data, ") >> HFILE
		else
			printf("\t    NULL, NULL, data, ") >> HFILE
	} else {
		printf("\treturn (dbenv->log_read_record(dbenv, \n") >> HFILE
		if (has_dbp)
			printf("\t    dbpp, td, data, ") >> HFILE
		else
			printf("\t    NULL, NULL, data, ") >> HFILE
	}

	printf("%s_desc, sizeof(%s_args), (void**)arg));\n}\n", logfunc, arg)\
						>> HFILE;
}

function print_function()
{
	# Write the print function; function prototype
	p[1] = sprintf("int %s_print", funcname);
	p[2] = " ";
	if (dbprivate)
		p[3] = "__P((ENV *, DBT *, DB_LSN *, db_recops, void *));";
	else
		p[3] = "__P((DB_ENV *, DBT *, DB_LSN *, db_recops));";
	p[4] = "";
	proto_format(p, PFILE);

	# Function declaration
	printf("int\n%s_print(%s, ", funcname, env_var) >> PFILE
	printf("dbtp, lsnp, notused2") >> PFILE
	if (dbprivate)
		printf(", info") >> PFILE
	printf(")\n") >> PFILE
	printf("\t%s *%s;\n", env_type, env_var) >> PFILE
	printf("\tDBT *dbtp;\n") >> PFILE
	printf("\tDB_LSN *lsnp;\n") >> PFILE
	printf("\tdb_recops notused2;\n") >> PFILE
	if (dbprivate)
		printf("\tvoid *info;\n") >> PFILE
	printf("{\n") >> PFILE

	# Get rid of complaints about unused parameters.
	if (dbprivate) {
		printf("\tCOMPQUIET(notused2, DB_TXN_PRINT);\n") >> PFILE
	} else {
		printf("\tnotused2 = DB_TXN_PRINT;\n") >> PFILE
	}
	printf("\n") >> PFILE

	printf(\
 	    "\treturn (__log_print_record(%senv, dbtp, lsnp, \"%s\", %s_desc", 
	     dbprivate ? "" : "dbenv->", funcname, funcname) >> PFILE

 	if (dbprivate)
 		printf(", info));\n") >> PFILE
 	else
 		printf(", NULL));\n") >> PFILE
 
	printf("}\n\n") >> PFILE
}


# proto_format --
#	Pretty-print a function prototype.
function proto_format(p, fp)
{
	printf("/*\n") >> fp;

	s = "";
	for (i = 1; i in p; ++i)
		s = s p[i];

	t = " * PUBLIC: "
	if (length(s) + length(t) < 80)
		printf("%s%s", t, s) >> fp;
	else {
		split(s, p, "__P");
		len = length(t) + length(p[1]);
		printf("%s%s", t, p[1]) >> fp

		n = split(p[2], comma, ",");
		comma[1] = "__P" comma[1];
		for (i = 1; i <= n; i++) {
			if (len + length(comma[i]) > 70) {
				printf("\n * PUBLIC:    ") >> fp;
				len = 0;
			}
			printf("%s%s", comma[i], i == n ? "" : ",") >> fp;
			len += length(comma[i]) + 2;
		}
	}
	printf("\n */\n") >> fp;
	delete p;
}

function make_name(unique_name, dup_name, p_version)
{
	logfunc = sprintf("%s_%s", prefix, unique_name);
	logname[num_funcs] = logfunc;
	if (is_compat) {
		funcname = sprintf("%s_%s_%s", prefix, unique_name, p_version);
	} else {
		funcname = logfunc;
	}

	if (is_duplicate)
		dupfuncs[num_funcs] = dup_name;
	else
		dupfuncs[num_funcs] = funcname;

	funcs[num_funcs] = funcname;
	functable[num_funcs] = is_compat;
	++num_funcs;
}

