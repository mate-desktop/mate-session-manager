if USE_NLS
PO_SUBDIR = po
endif

SUBDIRS = \
	$(PO_SUBDIR) \
	mate-submodules \
	mate-session \
	capplet \
	tools \
	data \
	doc

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-compile-warnings=no \
	CFLAGS='-Wno-deprecated-declarations'

EXTRA_DIST = \
	autogen.sh \
	HACKING

MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	$(srcdir)/configure \
	`find "$(srcdir)" -type f -name Makefile.in -print`

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
