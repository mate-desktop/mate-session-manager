bin_PROGRAMS = mate-session-save mate-session-inhibit

AM_CPPFLAGS =					\
	$(MATE_SESSION_CFLAGS)			\
	$(DBUS_GLIB_CFLAGS)			\
	$(MATECONF_FLAGS)				\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

mate_session_save_SOURCES =			\
	mate-session-save.c

mate_session_save_LDADD =			\
	$(SM_LIBS)				\
	$(ICE_LIBS)				\
	$(MATE_SESSION_LIBS)			\
	$(DBUS_GLIB_LIBS)			\
	$(MATECONF_LIBS)

mate_session_inhibit_SOURCES =			\
	mate-session-inhibit.c

mate_session_inhibit_CPPFLAGS =			\
	$(AM_CPPFLAGS)				\
	$(MATE_SESSION_CFLAGS)			\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)

mate_session_inhibit_LDADD =			\
	$(MATE_SESSION_LIBS)

-include $(top_srcdir)/git.mk
