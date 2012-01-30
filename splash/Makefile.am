NULL =

INCLUDES =					\
	-I$(top_srcdir)/egg			\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	$(WARN_CFLAGS)				\
	$(DISABLE_DEPRECATED_CFLAGS)		\
	$(SPLASH_CFLAGS)

LDADD =						\
	$(top_builddir)/egg/libeggsmclient-mate.la \
	$(SPLASH_LIBS)

helperdir = $(pkglibexecdir)/helpers
helper_PROGRAMS =				\
	mate-session-splash			\
	$(NULL)

default_sessiondir = $(sysconfdir)/xdg/autostart
default_session_in_in_files = mate-session-splash.desktop.in.in
default_session_in_files = $(default_session_in_in_files:.desktop.in.in=.desktop.in)
default_session_DATA = $(default_session_in_files:.desktop.in=.desktop)

mate_session_splash_SOURCES =			\
	mate-session-splash.c			\
	splash-window.c				\
	splash-window.h
mate_session_splash_LDADD =			\
	$(LDADD)

@INTLTOOL_DESKTOP_RULE@

%.desktop.in: %.desktop.in.in Makefile.am
	$(AM_V_GEN)sed -e 's,\@helperdir\@,$(helperdir),' \
		< $< > $@

EXTRA_DIST =					\
	README					\
	$(NULL)

CLEANFILES =					\
	$(default_session_DATA)			\
	$(default_session_in_files)

DISTCLEANFILES =					\
	$(default_session_in_in_files)

-include $(top_srcdir)/git.mk
