AC_INIT([telepathy-ring], [2.2.1])
AC_PREREQ([2.59])
AM_INIT_AUTOMAKE([foreign 1.9])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

AC_CONFIG_MACRO_DIR([m4])

AM_CONFIG_HEADER([config.h])

AC_PROG_CC
AC_PROG_CC_STDC

dnl Not yet
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

AC_ARG_ENABLE(mc-account-plugin,
              AS_HELP_STRING([--enable-mc-account-plugin=@<:@no/yes/auto@:>@],
                             [build MC account plugin]), , enable_mc_account_plugin=auto)

if test "x$enable_mc_account_plugin" != "xno"; then
    PKG_CHECK_MODULES(MCP,
        [
            mission-control-plugins
        ], have_mcp="yes", have_mcp="no")

    AC_MSG_CHECKING([Mission Control plugins dir])
    MISSION_CONTROL_PLUGINS_DIR=${libdir}/mission-control-plugins.`pkg-config --variable=MCP_ABI_VERSION mission-control-plugins`

    AC_MSG_RESULT([$MISSION_CONTROL_PLUGINS_DIR])
    AC_SUBST(MISSION_CONTROL_PLUGINS_DIR)
else
    have_mcp=no
fi

if test "x$enable_mc_account_plugin" = "xyes" -a "x$have_mcp" != "xyes"; then
    AC_MSG_ERROR([Could not find mission-control plugin dependencies:

$MCP_PKG_ERRORS])
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
PKG_CHECK_MODULES(TP, [telepathy-glib >= 0.11.14])

if pkg-config 'telepathy-glib >= 0.13.3' ||
   pkg-config 'telepathy-glib >= 0.12.2'
then
AC_DEFINE([HAVE_TP_MESSAGE_MIXIN_WITH_DELI], 1,
  [Do you have working DeliveryReportingSupport])
fi

if pkg-config 'telepathy-glib >= 0.13.10'
then
AC_DEFINE([HAVE_TP_SMS_CHANNEL], 1,
  [Do you have SMSChannel on Channel.Interface.SMS])
fi

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

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([modem/Makefile modem/tests/Makefile \
	         src/Makefile tests/Makefile docs/Makefile \
		 scripts/Makefile \
                 tools/Makefile ring-extensions/Makefile \
                 mc-plugin/Makefile])
AC_OUTPUT
