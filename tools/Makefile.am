bin_PROGRAMS = mate-session-save mate-session-inhibit
libexec_PROGRAMS = \
	mate-session-check-accelerated \
	mate-session-check-accelerated-gl-helper

AM_CPPFLAGS =					\
	$(MATE_SESSION_CFLAGS)			\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)

mate_session_save_SOURCES =			\
	mate-session-save.c

mate_session_save_LDADD =			\
	$(SM_LIBS)				\
	$(ICE_LIBS)				\
	$(MATE_SESSION_LIBS)

mate_session_inhibit_SOURCES =			\
	mate-session-inhibit.c

mate_session_inhibit_CPPFLAGS =			\
	$(AM_CPPFLAGS)				\
	$(MATE_SESSION_CFLAGS)			\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	$(DISABLE_DEPRECATED_CFLAGS)

mate_session_inhibit_LDADD =			\
	$(MATE_SESSION_LIBS)

if HAVE_GLESV2
libexec_PROGRAMS += mate-session-check-accelerated-gles-helper

mate_session_check_accelerated_gles_helper_SOURCES =	\
	mate-session-check-accelerated-common.h		\
	mate-session-check-accelerated-gles-helper.c

mate_session_check_accelerated_gles_helper_CPPFLAGS =	\
	-DPKGDATADIR=\""$(pkgdatadir)"\"		\
	$(AM_CPPFLAGS)					\
	$(GTK3_CFLAGS)					\
	$(GLES_TEST_CFLAGS)				\
	$(X11_CFLAGS)

mate_session_check_accelerated_gles_helper_LDADD =	\
	$(GLES_TEST_LIBS)				\
	$(GTK3_LIBS)					\
	$(X11_LIBS)
endif

mate_session_check_accelerated_gl_helper_SOURCES =	\
	mate-session-check-accelerated-common.h		\
	mate-session-check-accelerated-gl-helper.c

mate_session_check_accelerated_gl_helper_CPPFLAGS =	\
	-DPKGDATADIR=\""$(pkgdatadir)"\"		\
	$(AM_CPPFLAGS)					\
	$(GL_TEST_CFLAGS)				\
	$(X11_CFLAGS)

mate_session_check_accelerated_gl_helper_LDADD = 	\
	$(GL_TEST_LIBS)					\
	$(X11_LIBS)

mate_session_check_accelerated_SOURCES =       	\
	mate-session-check-accelerated-common.h	\
	mate-session-check-accelerated.c

mate_session_check_accelerated_CPPFLAGS =	\
	-DLIBEXECDIR=\""$(libexecdir)"\"	\
	$(AM_CPPFLAGS)				\
	$(GTK3_CFLAGS)				\
	$(GL_TEST_CFLAGS)			\
	$(X11_CPPFLAGS)

mate_session_check_accelerated_LDADD =	\
	$(GTK3_LIBS)			\
	$(GL_TEST_LIBS)			\
	$(X11_LIBS)

-include $(top_srcdir)/git.mk
