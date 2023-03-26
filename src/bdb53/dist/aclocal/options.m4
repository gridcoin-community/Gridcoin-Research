# $Id$

# Process user-specified options.
AC_DEFUN(AM_OPTIONS_SET, [

AC_MSG_CHECKING(if --enable-smallbuild option specified)
AC_ARG_ENABLE(smallbuild,
	[AC_HELP_STRING([--enable-smallbuild],
			[Build small footprint version of the library.])],
	[db_cv_smallbuild="$enable_smallbuild"], [db_cv_smallbuild="no"])
case "$db_cv_smallbuild" in
yes) db_cv_build_full="no";;
  *) db_cv_build_full="yes";;
esac
AC_MSG_RESULT($db_cv_smallbuild)

AC_MSG_CHECKING(if --disable-atomicsupport option specified)
AC_ARG_ENABLE(atomicsupport,
	AC_HELP_STRING([--disable-atomicsupport],
	    [Do not build any native atomic operation support.]),, enableval="yes")
db_cv_build_atomicsupport="$enableval"
case "$enableval" in
 no) AC_MSG_RESULT(yes);;
yes) AC_MSG_RESULT(no);;
esac

# --enable-bigfile was the configuration option that Berkeley DB used before
# autoconf 2.50 was released (which had --enable-largefile integrated in).
AC_ARG_ENABLE(bigfile,
	[AC_HELP_STRING([--disable-bigfile],
			[Obsolete; use --disable-largefile instead.])],
	[AC_MSG_ERROR(
	    [--enable-bigfile no longer supported, use --enable-largefile])])

AC_MSG_CHECKING(if --disable-compression option specified)
AC_ARG_ENABLE(compression,
	AC_HELP_STRING([--disable-compression],
	    [Do not build compression support.]),, enableval=$db_cv_build_full)
db_cv_build_compression="$enableval"
case "$enableval" in
 no) AC_MSG_RESULT(yes);;
yes) AC_MSG_RESULT(no);;
esac

AC_MSG_CHECKING(if --disable-hash option specified)
AC_ARG_ENABLE(hash,
	AC_HELP_STRING([--disable-hash],
	    [Do not build Hash access method.]),, enableval=$db_cv_build_full)
db_cv_build_hash="$enableval"
case "$enableval" in
 no) AC_MSG_RESULT(yes);;
yes) AC_MSG_RESULT(no);;
esac

AC_MSG_CHECKING(if --disable-heap option specified)
AC_ARG_ENABLE(heap,
	[AC_HELP_STRING([--disable-heap],
	    [Do not build Heap access method.])],, enableval=$db_cv_build_full)
db_cv_build_heap="$enableval"
case "$enableval" in
 no) AC_MSG_RESULT(yes);;
yes) AC_MSG_RESULT(no);;
esac

AC_MSG_CHECKING(if --disable-mutexsupport option specified)
AC_ARG_ENABLE(mutexsupport,
	AC_HELP_STRING([--disable-mutexsupport],
	    [Do not build any mutex support.]),, enableval="yes")
db_cv_build_mutexsupport="$enableval"
case "$enableval" in
 no) AC_MSG_RESULT(yes);;
yes) AC_MSG_RESULT(no);;
esac

AC_MSG_CHECKING(if --disable-log_checksum option specified)
AC_ARG_ENABLE(log_checksum,
	AC_HELP_STRING([--disable-log_checksum],
	    [Disable log checksums.]),
	[case "$enableval" in
	 no | yes) db_cv_log_checksum="$enableval" ;;
	  *) db_cv_log_checksum="yes" ;;
 	 esac], 
	db_cv_log_checksum="yes")
case "$db_cv_log_checksum" in
 no) AC_MSG_RESULT(yes);;
yes) AC_MSG_RESULT(no);;
esac


AC_MSG_CHECKING(if --disable-partition option specified)
AC_ARG_ENABLE(partition,
	AC_HELP_STRING([--disable-partition],
	    [Do not build partitioned database support.]),,
	enableval=$db_cv_build_full)
db_cv_build_partition="$enableval"
case "$enableval" in
 no) AC_MSG_RESULT(yes);;
yes) AC_MSG_RESULT(no);;
esac

AC_MSG_CHECKING(if --disable-queue option specified)
AC_ARG_ENABLE(queue,
	AC_HELP_STRING([--disable-queue],
	    [Do not build Queue access method.]),, enableval=$db_cv_build_full)
db_cv_build_queue="$enableval"
case "$enableval" in
 no) AC_MSG_RESULT(yes);;
yes) AC_MSG_RESULT(no);;
esac

AC_MSG_CHECKING(if --disable-replication option specified)
AC_ARG_ENABLE(replication,
	AC_HELP_STRING([--disable-replication],
	    [Do not build database replication support.]),,
	enableval=$db_cv_build_full)
db_cv_build_replication="$enableval"
case "$enableval" in
 no) AC_MSG_RESULT(yes);;
yes) AC_MSG_RESULT(no);;
esac

AC_MSG_CHECKING(if --disable-statistics option specified)
AC_ARG_ENABLE(statistics,
	AC_HELP_STRING([--disable-statistics],
	    [Do not build statistics support.]),, enableval=$db_cv_build_full)
db_cv_build_statistics="$enableval"
case "$enableval" in
 no) AC_MSG_RESULT(yes);;
yes) AC_MSG_RESULT(no);;
esac

AC_MSG_CHECKING(if --disable-verify option specified)
AC_ARG_ENABLE(verify,
	AC_HELP_STRING([--disable-verify],
	    [Do not build database verification support.]),,
	enableval=$db_cv_build_full)
db_cv_build_verify="$enableval"
case "$enableval" in
 no) AC_MSG_RESULT(yes);;
yes) AC_MSG_RESULT(no);;
esac

AC_MSG_CHECKING(if --enable-compat185 option specified)
AC_ARG_ENABLE(compat185,
	[AC_HELP_STRING([--enable-compat185],
			[Build DB 1.85 compatibility API.])],
	[db_cv_compat185="$enable_compat185"], [db_cv_compat185="no"])
AC_MSG_RESULT($db_cv_compat185)

AC_MSG_CHECKING(if --enable-cxx option specified)
AC_ARG_ENABLE(cxx,
	[AC_HELP_STRING([--enable-cxx],
			[Build C++ API.])],
	[db_cv_cxx="$enable_cxx"], [db_cv_cxx="no"])
AC_MSG_RESULT($db_cv_cxx)

AC_MSG_CHECKING(if --enable-debug option specified)
AC_ARG_ENABLE(debug,
	[AC_HELP_STRING([--enable-debug],
			[Build a debugging version.])],
	[db_cv_debug="$enable_debug"], [db_cv_debug="no"])
AC_MSG_RESULT($db_cv_debug)

AC_MSG_CHECKING(if --enable-debug_rop option specified)
AC_ARG_ENABLE(debug_rop,
	[AC_HELP_STRING([--enable-debug_rop],
			[Build a version that logs read operations.])],
	[db_cv_debug_rop="$enable_debug_rop"], [db_cv_debug_rop="no"])
AC_MSG_RESULT($db_cv_debug_rop)

AC_MSG_CHECKING(if --enable-debug_wop option specified)
AC_ARG_ENABLE(debug_wop,
	[AC_HELP_STRING([--enable-debug_wop],
			[Build a version that logs write operations.])],
	[db_cv_debug_wop="$enable_debug_wop"], [db_cv_debug_wop="no"])
AC_MSG_RESULT($db_cv_debug_wop)

AC_MSG_CHECKING(if --enable-diagnostic option specified)
AC_ARG_ENABLE(diagnostic,
	[AC_HELP_STRING([--enable-diagnostic],
			[Build a version with run-time diagnostics.])],
	[db_cv_diagnostic="$enable_diagnostic"], [db_cv_diagnostic="no"])
if test "$db_cv_diagnostic" = "yes"; then
	AC_MSG_RESULT($db_cv_diagnostic)
fi
if test "$db_cv_diagnostic" = "no" -a "$db_cv_debug_rop" = "yes"; then
	db_cv_diagnostic="yes"
	AC_MSG_RESULT([by --enable-debug_rop])
fi
if test "$db_cv_diagnostic" = "no" -a "$db_cv_debug_wop" = "yes"; then
	db_cv_diagnostic="yes"
	AC_MSG_RESULT([by --enable-debug_wop])
fi
if test "$db_cv_diagnostic" = "no"; then
	AC_MSG_RESULT($db_cv_diagnostic)
fi

AC_MSG_CHECKING(if --enable-dump185 option specified)
AC_ARG_ENABLE(dump185,
	[AC_HELP_STRING([--enable-dump185],
			[Build db_dump185(1) to dump 1.85 databases.])],
	[db_cv_dump185="$enable_dump185"], [db_cv_dump185="no"])
AC_MSG_RESULT($db_cv_dump185)

AC_MSG_CHECKING(if --enable-java option specified)
AC_ARG_ENABLE(java,
	[AC_HELP_STRING([--enable-java],
			[Build Java API.])],
	[db_cv_java="$enable_java"], [db_cv_java="no"])
AC_MSG_RESULT($db_cv_java)

AC_MSG_CHECKING(if --enable-mingw option specified)
AC_ARG_ENABLE(mingw,
	[AC_HELP_STRING([--enable-mingw],
			[Build Berkeley DB for MinGW.])],
	[db_cv_mingw="$enable_mingw"], [db_cv_mingw="no"])
AC_MSG_RESULT($db_cv_mingw)

AC_MSG_CHECKING(if --enable-o_direct option specified)
AC_ARG_ENABLE(o_direct,
	[AC_HELP_STRING([--enable-o_direct],
			[Enable the O_DIRECT flag for direct I/O.])],
	[db_cv_o_direct="$enable_o_direct"], [db_cv_o_direct="no"])
AC_MSG_RESULT($db_cv_o_direct)

AC_MSG_CHECKING(if --enable-posixmutexes option specified)
AC_ARG_ENABLE(posixmutexes,
	[AC_HELP_STRING([--enable-posixmutexes],
			[Force use of POSIX standard mutexes.])],
	[db_cv_posixmutexes="$enable_posixmutexes"], [db_cv_posixmutexes="no"])
AC_MSG_RESULT($db_cv_posixmutexes)

AC_ARG_ENABLE(pthread_self,,
	[AC_MSG_WARN([--enable-pthread_self is now always enabled])])

AC_ARG_ENABLE(pthread_api,,
	[AC_MSG_WARN([--enable-pthread_api is now always enabled])])

AC_MSG_CHECKING(if --enable-rpc option specified)
AC_ARG_ENABLE(rpc,,
	[AC_MSG_ERROR([RPC support has been removed from Berkeley DB.])]
	 , [db_cv_rpc="no"])
AC_MSG_RESULT($db_cv_rpc)

AC_MSG_CHECKING(if --enable-sql option specified)
AC_ARG_ENABLE(sql,
	[AC_HELP_STRING([--enable-sql],
			[Build the SQL API.])],
	[db_cv_sql="$enable_sql"], [db_cv_sql="no"])
AC_MSG_RESULT($db_cv_sql)

AC_MSG_CHECKING(if --enable-sql_compat option specified)
AC_ARG_ENABLE(sql_compat,
	[AC_HELP_STRING([--enable-sql_compat],
			[Build a drop-in replacement sqlite3 library.])],
	[db_cv_sql_compat="$enable_sql_compat"], [db_cv_sql_compat="no"])
AC_MSG_RESULT($db_cv_sql_compat)

AC_MSG_CHECKING(if --enable-jdbc option specified)
AC_ARG_ENABLE(jdbc,
	[AC_HELP_STRING([--enable-jdbc],
			[Build BDB SQL JDBC library.])],
	[db_cv_jdbc="$enable_jdbc"], [db_cv_jdbc="no"])
AC_MSG_RESULT($db_cv_jdbc)

AC_MSG_CHECKING([if --with-jdbc=DIR option specified])
AC_ARG_WITH(jdbc,
       [AC_HELP_STRING([--with-jdbc=DIR],
                       [Specify source directory of JDBC.])],
       [with_jdbc="$withval"], [with_jdbc="no"])
AC_MSG_RESULT($with_jdbc)
if test "$with_jdbc" != "no"; then
	db_cv_jdbc="yes"
fi

AC_MSG_CHECKING(if --enable-amalgamation option specified)
AC_ARG_ENABLE(amalgamation,
	AC_HELP_STRING([--enable-amalgamation],
	    [Build a SQL amalgamation instead of building files separately.]),
	[db_cv_sql_amalgamation="$enable_amalgamation"],
	[db_cv_sql_amalgamation="no"])
AC_MSG_RESULT($db_cv_sql_amalgamation)

AC_MSG_CHECKING(if --enable-sql_codegen option specified)
AC_ARG_ENABLE(sql_codegen,
	[AC_HELP_STRING([--enable-sql_codegen],
			[Build the SQL-to-C code generation tool.])],
	[db_cv_sql_codegen="$enable_sql_codegen"], [db_cv_sql_codegen="no"])
AC_MSG_RESULT($db_cv_sql_codegen)

AC_MSG_CHECKING(if --enable-stl option specified)
AC_ARG_ENABLE(stl,
	[AC_HELP_STRING([--enable-stl],
			[Build STL API.])],
	[db_cv_stl="$enable_stl"], [db_cv_stl="no"])
if test "$db_cv_stl" = "yes" -a "$db_cv_cxx" = "no"; then
	db_cv_cxx="yes"
fi
AC_MSG_RESULT($db_cv_stl)

AC_MSG_CHECKING(if --enable-tcl option specified)
AC_ARG_ENABLE(tcl,
	[AC_HELP_STRING([--enable-tcl],
			[Build Tcl API.])],
	[db_cv_tcl="$enable_tcl"], [db_cv_tcl="no"])
AC_MSG_RESULT($db_cv_tcl)

AC_MSG_CHECKING(if --enable-test option specified)
AC_ARG_ENABLE(test,
	[AC_HELP_STRING([--enable-test],
			[Configure to run the test suite.])],
	[db_cv_test="$enable_test"], [db_cv_test="no"])
AC_MSG_RESULT($db_cv_test)

AC_MSG_CHECKING(if --enable-localization option specified)
AC_ARG_ENABLE(localization,
	[AC_HELP_STRING([--enable-localization],
			[Configure to enable localization.])],
	[db_cv_localization="$enable_localization"], [db_cv_localization="no"])
AC_MSG_RESULT($db_cv_localization)

AC_MSG_CHECKING(if --enable-stripped_messages option specified)
AC_ARG_ENABLE(stripped_messages,
	[AC_HELP_STRING([--enable-stripped_messages],
			[Configure to enable stripped messages.])],
	[db_cv_stripped_messages="$enable_stripped_messages"], [db_cv_stripped_messages="no"])
AC_MSG_RESULT($db_cv_stripped_messages)

AC_MSG_CHECKING(if --enable-dbm option specified)
AC_ARG_ENABLE(dbm,
	[AC_HELP_STRING([--enable-dbm],
			[Configure to enable the historic dbm interface.])],
	[db_cv_dbm="$enable_dbm"], [db_cv_dbm="$db_cv_test"])
AC_MSG_RESULT($db_cv_dbm)

AC_MSG_CHECKING(if --enable-dtrace option specified)
AC_ARG_ENABLE(dtrace,
	[AC_HELP_STRING([--enable-dtrace],
			[Configure to build in dtrace static probes])],
	[db_cv_dtrace="$enable_dtrace"], [db_cv_dtrace="no"])
AC_MSG_RESULT($db_cv_dtrace)

AC_MSG_CHECKING(if --enable-systemtap option specified)
AC_ARG_ENABLE(systemtap,
	[AC_HELP_STRING([--enable-systemtap],
			[Configure to use systemtap to emulate dtrace static probes])],
	[db_cv_systemtap="$enable_systemtap"], [db_cv_systemtap="no"])
AC_MSG_RESULT($db_cv_systemtap)

AC_MSG_CHECKING(if --enable-perfmon-statistics option specified)
AC_ARG_ENABLE(perfmon_statistics,
	[AC_HELP_STRING([--enable-perfmon-statistics],
			[Configure to build in performance monitoring of statistics values  @<:@default=no@:>@.])],
	[db_cv_perfmon_statistics="$enable_perfmon_statistics"], [db_cv_perfmon_statistics="no"])
AC_MSG_RESULT($db_cv_perfmon_statistics)

AC_MSG_CHECKING(if --enable-uimutexes option specified)
AC_ARG_ENABLE(uimutexes,
	[AC_HELP_STRING([--enable-uimutexes],
			[Force use of Unix International mutexes.])],
	[db_cv_uimutexes="$enable_uimutexes"], [db_cv_uimutexes="no"])
AC_MSG_RESULT($db_cv_uimutexes)

AC_MSG_CHECKING(if --enable-umrw option specified)
AC_ARG_ENABLE(umrw,
	[AC_HELP_STRING([--enable-umrw],
			[Mask harmless uninitialized memory read/writes.])],
	[db_cv_umrw="$enable_umrw"], [db_cv_umrw="no"])
AC_MSG_RESULT($db_cv_umrw)

# Solaris, AI/X, OS/X and other BSD-derived systems default to POSIX-conforming
# disk i/o: A single read or write call is atomic. Other systems do not
# guarantee atomicity; in particular Linux and Microsoft Windows.
atomicfileread="no"
case "$host_os" in
solaris* | aix* | bsdi3* | freebsd* | darwin*)
	atomicfileread="yes";;
esac
AC_MSG_CHECKING(if --enable-atomicfileread option specified)
AC_ARG_ENABLE(atomicfileread,
	[AC_HELP_STRING([--enable-atomicfileread],
			[Indicate that the platform reads and writes files atomically.])],
	[db_cv_atomicfileread="$enable_atomicfileread"], [db_cv_atomicfileread=$atomicfileread])
AC_MSG_RESULT($db_cv_atomicfileread)
if test "$db_cv_atomicfileread" = "yes"; then
	AC_DEFINE(HAVE_ATOMICFILEREAD)
	AH_TEMPLATE(HAVE_ATOMICFILEREAD,
    [Define to 1 if platform reads and writes files atomically.])
fi

# Cryptography support.
# Until Berkeley DB 5.0, this was a simple yes/no decision.
# With the addition of support for Intel Integrated Performance Primitives (ipp)
# things are more complex.  There are now three options:
#   1) don't build cryptography (no)
#   2) build using the built-in software implementation (yes)
#   3) build using the Intel IPP implementation (ipp)
# We handle this by making the primary configuration method:
#   --with-cryptography={yes|no|ipp}
# which defaults to yes.  The old enable/disable-cryptography argument is still
# supported for backwards compatibility.
AC_MSG_CHECKING(if --with-cryptography option specified)
AC_ARG_ENABLE(cryptography, [], [], enableval=$db_cv_build_full)
enable_cryptography="$enableval"
AC_ARG_WITH([cryptography],
	AC_HELP_STRING([--with-cryptography=yes|no|ipp], [Build database cryptography support @<:@default=yes@:>@.]),
	[], [with_cryptography=$enable_cryptography])
case "$with_cryptography" in
yes|no|ipp) ;;
*) AC_MSG_ERROR([unknown --with-cryptography argument \'$with_cryptography\']) ;;
esac
db_cv_build_cryptography="$with_cryptography"
AC_MSG_RESULT($db_cv_build_cryptography)

AC_MSG_CHECKING(if --with-mutex=MUTEX option specified)
AC_ARG_WITH(mutex,
	[AC_HELP_STRING([--with-mutex=MUTEX],
			[Select non-default mutex implementation.])],
	[with_mutex="$withval"], [with_mutex="no"])
if test "$with_mutex" = "yes"; then
	AC_MSG_ERROR([--with-mutex requires a mutex name argument])
fi
if test "$with_mutex" != "no"; then
	db_cv_mutex="$with_mutex"
fi
AC_MSG_RESULT($with_mutex)

# --with-mutexalign=ALIGNMENT was the configuration option that Berkeley DB
# used before the DbEnv::mutex_set_align method was added.
AC_ARG_WITH(mutexalign,
	[AC_HELP_STRING([--with-mutexalign=ALIGNMENT],
			[Obsolete; use DbEnv::mutex_set_align instead.])],
	[AC_MSG_ERROR(
    [--with-mutexalign no longer supported, use DbEnv::mutex_set_align])])

AC_ARG_WITH(stacksize,
	[AC_HELP_STRING([--with-stacksize=SIZE],
			[Set the stack size for Berkeley DB threads.])],
	[with_stacksize="$withval"], [with_stacksize="no"])

AC_MSG_CHECKING([if --with-tcl=DIR option specified])
AC_ARG_WITH(tcl,
	[AC_HELP_STRING([--with-tcl=DIR],
			[Directory location of tclConfig.sh.])],
	[with_tclconfig="$withval"], [with_tclconfig="no"])
AC_MSG_RESULT($with_tclconfig)
if test "$with_tclconfig" != "no"; then
	db_cv_tcl="yes"
fi

AC_MSG_CHECKING([if --with-uniquename=NAME option specified])
AC_ARG_WITH(uniquename,
	[AC_HELP_STRING([--with-uniquename=NAME],
			[Build a uniquely named library.])],
	[with_uniquename="$withval"], [with_uniquename="no"])
if test "$with_uniquename" = "no"; then
	db_cv_uniquename="no"
	DB_VERSION_UNIQUE_NAME=""
	AC_MSG_RESULT($with_uniquename)
else
	db_cv_uniquename="yes"
	if test "$with_uniquename" = "yes"; then
		DB_VERSION_UNIQUE_NAME="__EDIT_DB_VERSION_UNIQUE_NAME__"
	else
		DB_VERSION_UNIQUE_NAME="$with_uniquename"
	fi
	AC_MSG_RESULT($DB_VERSION_UNIQUE_NAME)
fi

# Undocumented option used for the dbsql command line tool (to match SQLite).
AC_ARG_ENABLE(readline, [], [with_readline=$enableval], [with_readline=no])

# --enable-sql_compat implies --enable-sql
if test "$db_cv_sql_compat" = "yes" -a "$db_cv_sql" = "no"; then
	db_cv_sql=$db_cv_sql_compat
fi

# --enable-jdbc implies --enable-sql
if test "$db_cv_jdbc" = "yes" -a "$db_cv_sql" = "no"; then
	db_cv_sql=$db_cv_jdbc
fi

# Testing requires Tcl.
if test "$db_cv_test" = "yes" -a "$db_cv_tcl" = "no"; then
	AC_MSG_ERROR([--enable-test requires --enable-tcl])
fi])

