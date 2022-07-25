AC_INIT([mate-session-manager], [1.26.0], [https://mate-desktop.org/])
AC_CONFIG_SRCDIR([mate-session])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz check-news])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_CONFIG_MACRO_DIR([m4])

AM_MAINTAINER_MODE

MATE_COMMON_INIT
MATE_DEBUG_CHECK
MATE_COMPILE_WARNINGS

AC_PROG_CC
AC_STDC_HEADERS
AM_PROG_LIBTOOL
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_CONDITIONAL([USE_NLS], [test "x${USE_NLS}" = "xyes"])

dnl make sure we keep ACLOCAL_FLAGS around for maintainer builds to work
AC_SUBST(ACLOCAL_AMFLAGS, "\${ACLOCAL_FLAGS}")

MATE_MAINTAINER_MODE_DEFINES

AC_ARG_ENABLE(deprecations,
              [AS_HELP_STRING([--enable-deprecations],
                              [warn about deprecated usages @<:@default=no@:>@])],,
              [enable_deprecations=no])

if test "x$enable_deprecations" = "xyes"; then
   DISABLE_DEPRECATED_CFLAGS=$DISABLE_DEPRECATED
   AC_SUBST([DISABLE_DEPRECATED_CFLAGS])
fi

GLIB_REQUIRED=2.50.0
GIO_REQUIRED=2.25.0
GTK_REQUIRED=3.22.0
DBUS_GLIB_REQUIRED=0.76

dnl ====================================================================
dnl Dependency Checks
dnl ====================================================================
PKG_PROG_PKG_CONFIG()

PKG_CHECK_MODULES(MATE_SESSION,
        glib-2.0 >= $GLIB_REQUIRED
        gio-2.0 >= $GIO_REQUIRED
        gtk+-3.0 >= $GTK_REQUIRED
        dbus-glib-1 >= $DBUS_GLIB_REQUIRED
)

PKG_CHECK_MODULES(SESSION_PROPERTIES,
        glib-2.0 >= $GLIB_REQUIRED
        gtk+-3.0 >= $GTK_REQUIRED
)

PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(SM, sm)
PKG_CHECK_MODULES(ICE, ice)
PKG_CHECK_MODULES(XEXT, xext xau)
PKG_CHECK_MODULES(GTK3, gtk+-3.0 >= $GTK_REQUIRED)
PKG_CHECK_MODULES(GL_TEST, xcomposite gl glib-2.0 epoxy)

PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1 >= $DBUS_GLIB_REQUIRED)

PKG_CHECK_MODULES(LIBEGG, sm ice gtk+-3.0)

dnl ====================================================================
dnl GLESV2 (optional)
dnl ====================================================================

AC_ARG_WITH([libglesv2], AS_HELP_STRING([--without-libglesv2], [disable GLESV2 support]))
have_glesv2=no
if test x$with_libglesv2 != xno; then
    PKG_CHECK_MODULES(GLES_TEST, egl glesv2, have_glesv2=yes, have_glesv2=no)
fi
if test "x$have_glesv2" = "xyes"; then
  AC_DEFINE(HAVE_GLESV2, 1, [GLESV2 Support.])
fi
AM_CONDITIONAL([HAVE_GLESV2], [test "x$have_glesv2" = "xyes"])

dnl ====================================================================
dnl Option to set the default window manager
dnl ====================================================================
AC_ARG_WITH(default-wm,
              [AS_HELP_STRING([--with-default-wm],
                              [Specify the default window manager @<:@default=marco@:>@])],,
                              [with_default_wm="marco"])

DEFAULT_WM=$with_default_wm
AC_SUBST(DEFAULT_WM)
AM_CONDITIONAL(USE_MATE_WM, test x$with_default_wm = xmate-wm)

dnl ====================================================================
dnl GSettings
dnl ====================================================================
GLIB_GSETTINGS


dnl ====================================================================
dnl Systemd
dnl ====================================================================

AC_ARG_WITH(systemd,
            AS_HELP_STRING([--with-systemd],
            [Add systemd support]),,
            with_systemd=auto)

use_systemd=no
if test "x$with_systemd" != "xno" ; then
    PKG_CHECK_MODULES(SYSTEMD, [libsystemd], [use_systemd=yes],
                      [PKG_CHECK_MODULES(SYSTEMD, [libsystemd-login],
                      [use_systemd=yes], [use_systemd=no])])

    if test "x$use_systemd" = "xyes"; then
        AC_DEFINE(HAVE_SYSTEMD, 1, [systemd support])
        AC_SUBST(SYSTEMD_CFLAGS)
        AC_SUBST(SYSTEMD_LIBS)
    fi
fi
AM_CONDITIONAL(HAVE_SYSTEMD, test "x$use_systemd" = "xyes")
AC_SUBST(HAVE_SYSTEMD)

dnl ====================================================================
dnl Check for elogind
dnl
dnl elogind is not officially supported, but since it's a subset of
dnl systemd, it should work out of the box
dnl ====================================================================

AC_ARG_WITH(elogind,
            AS_HELP_STRING([--with-elogind],
            [Use libelogind instead of libsystemd-login]),,
            with_elogind=no)

use_elogind=no
if test "x$with_elogind" = "xyes"; then
    PKG_CHECK_MODULES(LIBELOGIND,[libelogind], [use_elogind=yes], [use_elogind=no])

    if test "x$use_elogind" = "xyes"; then
        AC_DEFINE([HAVE_ELOGIND], 1, [elogind support])
        AC_DEFINE([HAVE_SYSTEMD], 1, [Define to 1 to reduce ifdefs since elogind is a drop-in replacement for systemd])
        AC_SUBST(LIBELOGIND_CFLAGS)
        AC_SUBST(LIBELOGIND_LIBS)
    fi
fi
AM_CONDITIONAL(HAVE_ELOGIND, [test "x$use_elogind" = "xyes"])
AC_SUBST(HAVE_ELOGIND)

dnl ====================================================================
dnl Check for XSync extension
dnl ====================================================================

have_xsync=no
AC_CHECK_HEADER(X11/extensions/sync.h, [have_xsync=yes],,
                    [#include <X11/Xlib.h>])
if test "$have_xsync" = yes; then
  AC_DEFINE(HAVE_XSYNC, 1, [Have the SYNC extension library])
fi

dnl ====================================================================
dnl Check for XTest extension
dnl ====================================================================

have_xtest=no
PKG_CHECK_MODULES(XTEST, xtst, have_xtest=yes, have_xtest=no)
if test "$have_xtest" = yes; then
    AC_DEFINE(HAVE_XTEST, 1, [Have the XTest extension library])
fi
AC_SUBST(HAVE_XTEST)
AC_SUBST(XTEST_CFLAGS)
AC_SUBST(XTEST_LIBS)

dnl ====================================================================
dnl XRender checks
dnl ====================================================================

PKG_CHECK_MODULES(XRENDER, xrender, have_xrender=yes, have_xrender=no)
AM_CONDITIONAL(HAVE_XRENDER, test x$have_xrender = xyes)
if test $have_xrender=yes; then
    AC_DEFINE(HAVE_XRENDER, 1, [Have the Render X extension])
fi
AC_SUBST(HAVE_XRENDER)
AC_SUBST(XRENDER_CFLAGS)
AC_SUBST(XRENDER_LIBS)

dnl ====================================================================
dnl - DocBook Documentation
dnl ====================================================================

AC_ARG_ENABLE(docbook-docs,
	[AS_HELP_STRING([--enable-docbook-docs],
	[build documentation (requires xmlto)])],
	enable_docbook_docs=$enableval,enable_docbook_docs=auto)
AC_PATH_PROG(XMLTO, xmlto, no)
AC_MSG_CHECKING([whether to build DocBook documentation])
if test x$XMLTO = xno ; then
	have_docbook=no
else
	have_docbook=yes
fi
if test x$enable_docbook_docs = xauto ; then
	if test x$have_docbook = xno ; then
        	enable_docbook_docs=no
	else
		enable_docbook_docs=yes
	fi
fi
if test x$enable_docbook_docs = xyes; then
	if test x$have_docbook = xno; then
		AC_MSG_ERROR([Building DocBook docs explicitly required, but DocBook not found])
	fi
fi
AM_CONDITIONAL(DOCBOOK_DOCS_ENABLED, test x$enable_docbook_docs = xyes)
AC_MSG_RESULT($enable_docbook_docs)

dnl ====================================================================
dnl Check for xsltproc
dnl ====================================================================
AC_PATH_PROG([XSLTPROC], [xsltproc])

dnl ====================================================================
dnl Language Support
dnl ====================================================================
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The gettext translation domain])
AC_SUBST(GETTEXT_PACKAGE)

dnl ====================================================================
dnl Headers
dnl ====================================================================
AC_HEADER_STDC
AC_CHECK_HEADERS(syslog.h tcpd.h sys/param.h)

dnl ====================================================================
dnl check for backtrace support
dnl ====================================================================

AC_CHECK_HEADERS(execinfo.h)
AC_CHECK_LIB(execinfo, backtrace, [have_backtrace="yes"], [have_backtrace="no"])
EXECINFO_LIBS=""
if test "x$have_backtrace" = "xyes"; then
        EXECINFO_LIBS="-lexecinfo"
fi
AC_SUBST(EXECINFO_LIBS)

dnl ====================================================================
dnl Check for X transport interface - allows to disable ICE Transports
dnl See also https://bugzilla.gnome.org/show_bug.cgi?id=725100
dnl ====================================================================
AC_ARG_WITH([xtrans],
	    [AS_HELP_STRING([--without-xtrans],
			   [Build without xtrans support (results in ICE listening on remote TCP ports)])],
	    [],
	    [with_xtrans=yes])
AS_IF([test "$with_xtrans" != "no"],
      [PKG_CHECK_MODULES(XTRANS, xtrans,
      [AC_DEFINE(HAVE_XTRANS, 1, [Have the X Transport library])])])
AC_SUBST(HAVE_XTRANS)

dnl ====================================================================
dnl Code for checking whether IPv6 is enabled on the system....
dnl ====================================================================
AC_MSG_CHECKING([whether to enable ipv6])
AC_ARG_ENABLE(ipv6,
              AS_HELP_STRING([--enable-ipv6], [enable IPv6 extensions]),,
              [enable_ipv6=yes])
have_full_ipv6=no
if test $enable_ipv6 = yes; then

  dnl ====================================================================
  dnl Code for checking presence of AF_INET6 on the system....
  dnl ====================================================================
  AC_TRY_COMPILE([
    #include <sys/types.h>
    #include <sys/socket.h>], [
    socket(AF_INET6, SOCK_STREAM, 0)
    ],
    have_ipv6=yes,
    have_ipv6=no
  )
  AC_MSG_RESULT($have_ipv6)

  dnl =================================================================
  dnl Now we would check for specific function like getaddrinfo.
  dnl =================================================================
  have_getaddrinfo=no
  if test $have_ipv6=yes; then
    AC_CHECK_FUNC(getaddrinfo, have_getaddrinfo=yes)
    if test $have_getaddrinfo != yes; then
      # getaddrinfo is not in the default libraries.  See if it's in some other.
      for lib in bsd socket inet; do
        AC_CHECK_LIB($lib, getaddrinfo, [LIBS="$LIBS -l$lib";have_getaddrinfo=yes; break])
      done
    fi
    if test $have_getaddrinfo=yes; then
      AC_DEFINE(ENABLE_IPV6, 1, [Define if IPV6 is supported])
      have_full_ipv6=yes
    fi
  fi
fi
dnl ==============================================================================
dnl End of IPv6 checks
dnl ==============================================================================

# Don't use AC_PROG_AWK since we need the full pathname.
AC_PATH_PROGS(AWK, mawk gawk nawk awk, )
AC_PATH_PROGS(PERL, perl5 perl)

# define a MAINT-like variable REBUILD which is set if Perl
# and awk are found, so autogenerated sources can be rebuilt
AC_ARG_ENABLE(rebuilds,
              AS_HELP_STRING([--disable-rebuilds],
                             [disable all source autogeneration rules]),,
              [enable_rebuilds=yes])
REBUILD=\#
if test "x$enable_rebuilds" = "xyes" && \
     test -n "$PERL" && \
     $PERL -e 'exit !($] >= 5.002)' > /dev/null 2>&1 && \
     test -n "$AWK" ; then
  REBUILD=
fi
AC_SUBST(REBUILD)

AC_CONFIG_FILES([
Makefile
capplet/Makefile
doc/Makefile
doc/dbus/Makefile
doc/dbus/mate-session.xml
doc/man/Makefile
data/Makefile
data/org.mate.session.gschema.xml
data/icons/Makefile
mate-submodules/Makefile
mate-submodules/libegg/Makefile
mate-session/Makefile
tools/Makefile
po/Makefile.in
])
AC_OUTPUT

dnl ---------------------------------------------------------------------------
dnl - Show summary
dnl ---------------------------------------------------------------------------

echo "
              mate-session $VERSION
              `echo mate-session $VERSION | sed "s/./=/g"`

        prefix:                   ${prefix}
        exec_prefix:              ${exec_prefix}
        libdir:                   ${libdir}
        bindir:                   ${bindir}
        sbindir:                  ${sbindir}
        sysconfdir:               ${sysconfdir}
        localstatedir:            ${localstatedir}
        datadir:                  ${datadir}
        source code location:     ${srcdir}
        compiler:                 ${CC}
        cflags:                   ${CFLAGS}
        Warning flags:            ${WARN_CFLAGS}
        Maintainer mode:          ${USE_MAINTAINER_MODE}
        Warn about deprecations:  ${enable_deprecations}

        Default WM:               ${with_default_wm}
        Systemd support:          ${use_systemd}
        Elogind support:          ${use_elogind}
        GLES2 support:            ${have_glesv2}
        IPv6 support:             ${have_full_ipv6}
        Backtrace support:        ${have_backtrace}
        XRender support:          ${have_xrender}
        XSync support:            ${have_xsync}
        XTest support:            ${have_xtest}
        Build documentation:      ${enable_docbook_docs}
        Native Language support:  ${USE_NLS}

"
