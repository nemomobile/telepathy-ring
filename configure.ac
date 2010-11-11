AC_INIT([telepathy-ring], [2.1.0])
AC_PREREQ([2.59])
AM_INIT_AUTOMAKE([foreign 1.9])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

AC_CONFIG_MACRO_DIR([m4])

AM_CONFIG_HEADER([config.h])

AC_PROG_CC
AC_PROG_CC_STDC

dnl Not yet
AC_DISABLE_SHARED
AM_PROG_LIBTOOL

dnl decide on error flags
AS_COMPILER_FLAG(-Wall, [
  ERROR_CFLAGS="-Wall"
  if test "x$WERROR" = "xyes"; then
    AS_COMPILER_FLAG(-Werror,ERROR_CFLAGS="$ERROR_CFLAGS -Werror",ERROR_CFLAGS="$ERROR_CFLAGS")
  fi
])
AC_SUBST(ERROR_CFLAGS)

AM_MAINTAINER_MODE

AC_ARG_ENABLE(debug,
  AC_HELP_STRING([--disable-debug],[omit debug code]),,
  enable_debug=yes)

if test x$enable_debug = xyes; then
  AC_DEFINE([ENABLE_DEBUG], [1], [Enable debug code])
fi

AC_HEADER_STDC
AC_C_INLINE

dnl GTK docs
dnl GTK_DOC_CHECK

AC_DEFINE([_GNU_SOURCE], [1], [Enable GNU extensions])

PKG_CHECK_MODULES([GLIB], [gobject-2.0 >= 2.4 glib-2.0])

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])

PKG_CHECK_MODULES([DBUS], [dbus-1 >= 0.60, dbus-glib-1 >= 0.88])

PKG_CHECK_MODULES([UUID], [uuid])

AS_AC_EXPAND(DATADIR, ${datadir})
DBUS_SERVICES_DIR="$DATADIR/dbus-1/services"
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED([DBUS_SERVICES_DIR], "$DBUS_SERVICES_DIR", [DBus services directory])

dnl Check for telepathy-glib
PKG_CHECK_MODULES(TP, [telepathy-glib >= 0.11.11])

AC_CHECK_PROGS([XSLTPROC], [$XSLTPROC xsltproc])
AC_CHECK_PROGS([DBUS_BINDING_TOOL], [$DBUS_BINDING_TOOL dbus-binding-tool])
AC_CHECK_PROGS([PYTHON], [$PYTHON python2.5 python2.4 python2.3 python])

AM_CONDITIONAL([HAVE_TP_EXTENSIONS], [test -n "$XSLTPROC" && test -n "$PYTHON" && test -n "$DBUS_BINDING_TOOL"])

if ! test -z "$XSLTPROC""$PYTHON""$DBUS_BINDING_TOOL"; then
   AC_DEFINE([HAVE_TP_EXTENSIONS], 1, [Can you generate extensions])
fi

AC_CHECK_FUNC([getresuid])

AC_SEARCH_LIBS(pthread_mutex_trylock, pthread,,
 AC_ERROR([POSIX threads not available]))
AC_CHECK_LIB([mlocknice], [mln_lock_data],,
  AC_MSG_WARN([Library mlocknice not found]))

AC_SUBST([testdir],'${libdir}/tests/telepathy-ring-tests')
AC_SUBST([includemodemdir],'${includedir}/modem-glib')
AC_SUBST([includesmsdir],'${includedir}/sms-glib')

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([modem/Makefile modem/tests/Makefile \
	         src/Makefile tests/Makefile docs/Makefile \
		 scripts/Makefile sms-glib/Makefile \
                 tools/Makefile ring-extensions/Makefile])
AC_OUTPUT
