dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.52)
AC_INIT([pxlib], [0.6.9], http://sourceforge.net/projects/pxlib)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/paradox.c])
AM_INIT_AUTOMAKE()

AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

dnl Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_INTLTOOL

dnl the following sets the soname version
PXLIB_MAJOR_VERSION=0
PXLIB_MINOR_VERSION=6
PXLIB_MICRO_VERSION=9
PXLIB_VERSION=$PXLIB_MAJOR_VERSION.$PXLIB_MINOR_VERSION.$PXLIB_MICRO_VERSION
PXLIB_VERSION_INFO=`expr $PXLIB_MAJOR_VERSION + $PXLIB_MINOR_VERSION`:$PXLIB_MICRO_VERSION:$PXLIB_MINOR_VERSION

AC_SUBST(PXLIB_MAJOR_VERSION)
AC_SUBST(PXLIB_MINOR_VERSION)
AC_SUBST(PXLIB_MICRO_VERSION)
AC_SUBST(PXLIB_VERSION)
AC_SUBST(PXLIB_VERSION_INFO)

dnl VERSION=${PXLIB_VERSION}

if test -n "$SOURCE_DATE_EPOCH"; then
	PXLIB_BUILD_DATE=`date --utc --date="@$SOURCE_DATE_EPOCH" '+%Y-%m-%d'`
else
	PXLIB_BUILD_DATE=`date '+%Y-%m-%d'`
fi
AC_DEFINE_UNQUOTED(PXLIB_BUILD_DATE,"$PXLIB_BUILD_DATE",[pxlib build date])
AC_SUBST(PXLIB_BUILD_DATE)

dnl Add the languages which your application supports here.
ALL_LINGUAS="de es fr"
dnl AM_GNU_GETTEXT
dnl AM_GNU_GETTEXT_VERSION(0.19.8)
GETTEXT_PACKAGE=pxlib
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", GETTEXT_PACKAGE)

AC_PROG_LIBTOOL

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN
AC_STRUCT_TM

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS(iconv.h fcntl.h unistd.h ctype.h dirent.h errno.h malloc.h)
AC_CHECK_HEADERS(stdbool.h stdarg.h sys/stat.h sys/types.h time.h)
AC_CHECK_HEADERS(ieeefp.h nan.h math.h fp_class.h float.h)
AC_CHECK_HEADERS(stdlib.h sys/socket.h netinet/in.h arpa/inet.h)
AC_CHECK_HEADERS(netdb.h sys/time.h sys/select.h sys/mman.h)

dnl Checks for library functions.
AC_FUNC_STRFTIME
AC_CHECK_FUNCS(strdup strndup strerror snprintf)
AC_CHECK_FUNCS(finite isnand fp_class class fpclass)
AC_CHECK_FUNCS(strftime localtime)

dnl Checks for inet libraries:
AC_CHECK_FUNC(gethostent, , AC_CHECK_LIB(nsl, gethostent))
AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt))
AC_CHECK_FUNC(connect, , AC_CHECK_LIB(inet, connect))

dnl Determine what socket length (socklen_t) data type is
AC_MSG_CHECKING([for type of socket length (socklen_t)])

dnl PX_LIBDIR='-L${libdir}'
dnl PX_INCLUDEDIR='-I${includedir}/pxlib -I${includedir}'
dnl PX_LIBS="-lpx $M_LIBS $LIBS"

dnl
dnl Extra flags
dnl
PX_CFLAGS=""

PX_HAVE_ICONV=0
PX_HAVE_RECODE=0

dnl AM_ICONV

dnl AM_ICONV_LINK

if test "$am_cv_func_iconv" != "yes" ; then
	AC_ARG_WITH(recode, [  --with-recode=DIR        Path to recode library (/usr)])
	if test -r ${withval}/include/recode.h ; then
		RECODE_LIBDIR=-L${withval}/lib
		RECODE_INCLUDEDIR=-I${withval}/include 
	else
		RECODE_LIBDIR=-L/usr
		RECODE_INCLUDEDIR=-I/usr/include 
	fi

	AC_CHECK_LIB(recode, recode_new_outer,
		[ RECODE_LIBDIR="$RECODE_LIBDIR -lrecode";
		RECODE_LIBS=-lrecode;
		PX_HAVE_RECODE=1;
		AC_DEFINE(HAVE_RECODE, 1, [Define if you have the recode library.])
		],[
		AC_MSG_RESULT([librecode not found])
		], "$RECODE_LIBDIR")

else
	PX_HAVE_ICONV=1
	AC_MSG_CHECKING([for recode])
	AC_MSG_RESULT([not needed, using iconv])
fi

AC_ARG_WITH(gsf, [  --with-gsf=DIR           Path to gsf library (/usr)])
if test "x$withval" != xno; then
	PKG_CHECK_MODULES(GSF_DEPS, libgsf-1, [
			AM_CONDITIONAL(HAVE_GSF, true)
			AC_DEFINE(HAVE_GSF, [1], [Define to 1 if we can use gsf libraries])
			PX_CFLAGS="$PX_CFLAGS $GSF_DEPS_CFLAGS"
			GSF_LIBS="$GSF_DEPS_LIBS"
			use_gsf=yes
		],[
			AM_CONDITIONAL(HAVE_GSF, false)
			use_gsf=no
		]
	)
else
	AM_CONDITIONAL(HAVE_GSF, false)
	use_gsf=no
fi

AC_CHECK_PROG(DOC_TO_MAN, docbook-to-man, docbook-to-man)
if test ! "$DOC_TO_MAN" ; then
	DOCDIR=
	AC_MSG_RESULT([docbook-to-man could not be found. I will not build man pages!])
else
	DOCDIR=doc
fi

AC_ARG_WITH(debug, [  --with-debug            Add the debugging module (on)])
if test "$with_debug" = "no" ; then
    echo Disabling DEBUG support
    WITH_DEBUG=0
    DEBUG_OBJ=
else    
    WITH_DEBUG=1
    DEBUG_OBJ=debugPX.o
fi
AC_SUBST(WITH_DEBUG)
AC_SUBST(DEBUG_OBJ)

AC_SUBST(CFLAGS)
AC_SUBST(PX_CFLAGS)

dnl AC_SUBST(PX_LIBDIR)
dnl AC_SUBST(PX_LIBS)
dnl AC_SUBST(PX_INCLUDEDIR)

AC_SUBST(RECODE_LIBDIR)
AC_SUBST(RECODE_LIBS)
AC_SUBST(RECODE_INCLUDEDIR)

AC_SUBST(PX_HAVE_RECODE)
AC_SUBST(PX_HAVE_ICONV)

dnl AC_SUBST(GSF_LIBDIR)
AC_SUBST(GSF_LIBS)
dnl AC_SUBST(GSF_INCLUDEDIR)
AC_SUBST(GSF_DEPS_CFLAGS)

AC_SUBST(HAVE_GSF)

AC_SUBST(M_LIBS)

AC_SUBST(DOCDIR)
AC_SUBST(DOC_TO_MAN)

dnl AC_DEFINE_UNQUOTED(PXLIB_MAJOR_VERSION, "$PXLIB_MAJOR_VERSION", PXLIB_MAJOR_VERSION)
dnl AC_DEFINE_UNQUOTED(PXLIB_MINOR_VERSION, "$PXLIB_MINOR_VERSION", PXLIB_MINOR_VERSION)
dnl AC_DEFINE_UNQUOTED(PXLIB_MICRO_VERSION, "$PXLIB_MICRO_VERSION", PXLIB_MICRO_VERSION)

AC_OUTPUT([
Makefile
pxlib.spec
pxlib.pc
include/Makefile
src/Makefile
po/Makefile.in
doc/Makefile
include/pxversion.h
include/paradox.h
include/paradox-gsf.h
])

echo
echo "Configuration:"
echo "  Prefix:     $prefix"
echo -n "  Recoding:   "
if test "$PX_HAVE_ICONV" = "1" ; then
	echo "Using iconv for recoding.";
else
	if test "$PX_HAVE_RECODE" = "1" ; then
		echo "Using recode for recoding.";
	else
		echo "No support for recoding.";
	fi
fi
echo
