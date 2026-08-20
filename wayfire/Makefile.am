sessiondatadir = $(pkgdatadir)
sessiondata_DATA = wayfire.ini

wayfire.ini: wayfire.ini.in
	$(AM_V_GEN) cp $(srcdir)/wayfire.ini.in $@

waylandsessionsdir = $(datadir)/wayland-sessions
waylandsession_in_files = mate-wayland.desktop.in
waylandsessions_DATA = $(waylandsession_in_files:.desktop.in=.desktop)

schemasdir = $(datadir)/glib-2.0/schemas
schemas_DATA = 10_mate-wayland.gschema.override

%.desktop: %.desktop.in Makefile
if USE_NLS
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
else
	$(AM_V_GEN) sed '/^# Translators/d' < $< > $@
endif

EXTRA_DIST =				\
	$(waylandsession_in_files)	\
	wayfire.ini.in			\
	$(schemas_DATA)

CLEANFILES =				\
	$(waylandsessions_DATA)		\
	wayfire.ini

-include $(top_srcdir)/git.mk
