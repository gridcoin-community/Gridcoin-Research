# $Id$

# Determine what kind of application-specified performance event monitoring
# support is available for this platform. The options are:
#	--enable-dtrace	is supported on
#		Solaris
#		Linux	 SystemTap 1.1 or better
#		Mac OS X version 10.5 (Leopard) or better

AC_DEFUN(AM_DEFINE_PERFMON, [

AH_TEMPLATE(HAVE_PERFMON,
    [Define to 1 to enable some kind of performance event monitoring.])
AH_TEMPLATE(HAVE_PERFMON_STATISTICS,
    [Define to 1 to enable performance event monitoring of *_stat() statistics.])
AH_TEMPLATE(HAVE_DTRACE,
    [Define to 1 to use dtrace for performance monitoring.])

if test "$db_cv_systemtap" = "yes" ; then
	if test "$DTRACE" != "dtrace"; then
		AC_MSG_ERROR([The dtrace program is missing; is systemtap v1.1 or better installed?])
	fi
	db_cv_dtrace="yes"
fi
if test "$db_cv_dtrace" = "yes" ; then
	db_cv_perfmon="yes"
fi

AC_SUBST(DTRACE_CPP)
DTRACE_CPP=-C
if test "$db_cv_perfmon" = "yes" ; then
    if test "$DTRACE" = "dtrace" ; then
		AC_CHECK_HEADERS(sys/sdt.h)
		# Generate the DTrace provider header file. This is duplicated
		# in Makefile.in, to allow custom events to be added.
		if test "$STAP" = "stap"; then
		    # Linux DTrace support may have a bug with dtrace -C -h
		    # The preprocessing isn't needed for -h on Linux,
		    # so skip the unnecessary preprocessing.
		    DTRACE_CPP=
		fi
		# The OS X version of dtrace prints a spurious line here.
		if ! dtrace -h $DTRACE_CPP -I../util/dtrace -s ../dist/db_provider.d; then
		    AC_MSG_ERROR([Could not build db_provider.d: dtrace -h failed])
		fi
		$RM db_provider.h.tmp
		if ! mv db_provider.h db_provider.h.tmp ; then
		    AC_MSG_ERROR([Could not build db_provider.d: mv failed])
		elif ! sed -e \
'/^#define[ 	]*BDB_[A-Z_]*(.*)/y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/' \
db_provider.h.tmp > db_provider.h ; then
		    AC_MSG_ERROR([Could not build db_provider.d: sed failed])
		fi

		# DTrace on Solaris needs to post-process .o files to both
		# generate an additional .o as well as resolving the
		# __dtrace___bdb___<xxx> symbols before putting them into
		# libraries;  Mac OS X does not. Treat a failing dtrace -G
		# command as the indicator sign that dtrace -G is unnecessary.
		# If it is needed then create an empty .c file to be a
		# placeholder for the PIC & non-PIC versions of the dtrace -G
		# output file.  The root of this .c file must be the same as
		# the root of the .d file -- i.e. db_provider -- for the
		# dtrace -G lines at the end of Makefile.in to work correctly. 
		$RM db_provider.o
		if dtrace -G $DTRACE_CPP -I../util/dtrace -s ../dist/db_provider.d 2> /dev/null && \
		    test -f db_provider.o ; then
			FINAL_OBJS="$FINAL_OBJS db_provider${o}"
			rm -f db_provider.c
			echo "" > db_provider.c
		fi
		AC_DEFINE(HAVE_DTRACE)
	else
		AC_MSG_ERROR([No supported performance utility found.])
	fi
	AC_DEFINE(HAVE_PERFMON)
	if test "$db_cv_perfmon_statistics" != "no" ; then
		AC_DEFINE(HAVE_PERFMON_STATISTICS)
	fi
	# The method by which probes are listed depends on the underlying
	# implementation; Linux's emulation of DTrace still uses the stap
	# command at runtime.
	AC_SUBST(LISTPROBES_DEPENDENCY)
	AC_SUBST(LISTPROBES_COMMAND)
        if test "$STAP" = "stap"; then
		LISTPROBES_DEPENDENCY=.libs/libdb-$DB_VERSION_MAJOR.$DB_VERSION_MINOR$SOSUFFIX
		LISTPROBES_COMMAND="stap -l 'process(\"$LISTPROBES_DEPENDENCY\").mark(\"*\")'"
	elif test "$DTRACE" = "dtrace" ; then
		LISTPROBES_DEPENDENCY=db_load
		LISTPROBES_COMMAND="dnl
	# Run a simple command which uses the library without needing any setup.
	sleep 1 | dtrace -l -n 'bdb\$\$target:::'  -c '.libs/db_load dummy.db'"
        fi
elif test "$db_cv_perfmon_statistics" = "yes" ; then
	AC_MSG_ERROR([Enabling perfmon statistics requires --enable-dtrace])
fi
])
