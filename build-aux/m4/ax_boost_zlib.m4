# SYNOPSIS
#
#   AX_BOOST_ZLIB
#
# DESCRIPTION
#
#   Test for ZLib library from the Boost C++ libraries. The macro
#   requires a preceding call to AX_BOOST_BASE. Further documentation is
#   available at <http://randspringer.de/boost/index.html>.
#
#   This macro calls:
#
#     AC_SUBST(BOOST_ZLIB_LIB)
#
#   And sets:
#
#     HAVE_BOOST_ZLIB
#
# LICENSE
#
#   Copyright (c) 2008 Thomas Porschberg <thomas@randspringer.de>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 21

AC_DEFUN([AX_BOOST_ZLIB],
[
	AC_ARG_WITH([boost-zlib],
	AS_HELP_STRING([--with-boost-zlib@<:@=special-lib@:>@],
                   [use the ZLib library from boost - it is possible to specify a certain library for the linker
                        e.g. --with-boost-zlib=boost_zlib-gcc-mt-d-1_33_1 ]),
        [
        if test "$withval" = "no"; then
			want_boost="no"
        elif test "$withval" = "yes"; then
            want_boost="yes"
            ax_boost_user_zlib_lib=""
        else
		    want_boost="yes"
		ax_boost_user_zlib_lib="$withval"
		fi
        ],
        [want_boost="yes"]
	)

	if test "x$want_boost" = "xyes"; then
        AC_REQUIRE([AC_PROG_CC])
		CPPFLAGS_SAVED="$CPPFLAGS"
		CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
		export CPPFLAGS

		LDFLAGS_SAVED="$LDFLAGS"
		LDFLAGS="$LDFLAGS $BOOST_LDFLAGS"
		export LDFLAGS

			AC_DEFINE(HAVE_BOOST_ZLIB,,[define if the Boost::zlib library is available])
            BOOSTLIBDIR=`echo $BOOST_LDFLAGS | sed -e 's/@<:@^\/@:>@*//'`
            if test "x$ax_boost_user_zlib_lib" = "x"; then
                for libextension in `ls $BOOSTLIBDIR/libboost_zlib*.so* $BOOSTLIBDIR/libboost_iostream*.dylib* $BOOSTLIBDIR/libboost_zlib*.a* 2>/dev/null | sed 's,.*/,,' | sed -e 's;^lib\(boost_zlib.*\)\.so.*$;\1;' -e 's;^lib\(boost_iostream.*\)\.dylib.*$;\1;' -e 's;^lib\(boost_zlib.*\)\.a.*$;\1;'` ; do
                     ax_lib=${libextension}
				    AC_CHECK_LIB($ax_lib, exit,
                                 [BOOST_ZLIB_LIB="-l$ax_lib"; AC_SUBST(BOOST_ZLIB_LIB) link_zlib="yes"; break],
                                 [link_zlib="no"])
				done
                if test "x$link_zlib" != "xyes"; then
                for libextension in `ls $BOOSTLIBDIR/boost_zlib*.dll* $BOOSTLIBDIR/boost_zlib*.a* 2>/dev/null | sed 's,.*/,,' | sed -e 's;^\(boost_zlib.*\)\.dll.*$;\1;' -e 's;^\(boost_zlib.*\)\.a.*$;\1;'` ; do
                     ax_lib=${libextension}
				    AC_CHECK_LIB($ax_lib, exit,
                                 [BOOST_ZLIB_LIB="-l$ax_lib"; AC_SUBST(BOOST_ZLIB_LIB) link_zlib="yes"; break],
                                 [link_zlib="no"])
				done
                fi

            else
               for ax_lib in $ax_boost_user_zlib_lib boost_zlib-$ax_boost_user_zlib_lib; do
				      AC_CHECK_LIB($ax_lib, main,
                                   [BOOST_ZLIB_LIB="-l$ax_lib"; AC_SUBST(BOOST_ZLIB_LIB) link_zlib="yes"; break],
                                   [link_zlib="no"])
                  done

            fi

		CPPFLAGS="$CPPFLAGS_SAVED"
	LDFLAGS="$LDFLAGS_SAVED"
	fi
])
