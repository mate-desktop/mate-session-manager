SUBDIRS = \
	egg \
	mate-session \
	capplet \
	tools \
	data \
	doc \
	po

if ENABLE_SPLASH
SUBDIRS += splash
endif

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	HACKING \
	MAINTAINERS

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
	$(srcdir)/m4/intltool.m4 \
	`find "$(srcdir)" -type f -name Makefile.in -print`

-include $(top_srcdir)/git.mk
