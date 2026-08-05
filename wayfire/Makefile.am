bin_SCRIPTS = mate-wayland-session

mate-wayland-session: mate-wayland-session.in Makefile
	$(AM_V_GEN) sed -e 's|@MATE_WAYLAND_DATADIR@|$(pkgdatadir)|g' $(srcdir)/mate-wayland-session.in > $@
	chmod +x $@

sessiondatadir = $(pkgdatadir)
sessiondata_DATA = wayfire.ini

wayfire.ini: wayfire.ini.in
	$(AM_V_GEN) cp $(srcdir)/wayfire.ini.in $@

xsessiondir = $(datadir)/xsessions
xsession_in_files = mate-wayland.desktop.in
xsession_DATA = $(xsession_in_files:.desktop.in=.desktop)

schemasdir = $(datadir)/glib-2.0/schemas
schemas_DATA = 10_mate-wayland.gschema.override

%.desktop: %.desktop.in Makefile
if USE_NLS
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
else
	$(AM_V_GEN) sed '/^# Translators/d' < $< > $@
endif

EXTRA_DIST =				\
	$(xsession_in_files)		\
	mate-wayland-session.in		\
	wayfire.ini.in			\
	$(schemas_DATA)

CLEANFILES =				\
	$(xsession_DATA)		\
	mate-wayland-session		\
	wayfire.ini

-include $(top_srcdir)/git.mk
