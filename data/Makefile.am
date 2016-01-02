SUBDIRS = icons

bin_SCRIPTS = mate-wm

uidir = $(pkgdatadir)
ui_DATA = \
	session-properties.ui	\
	gsm-inhibit-dialog.ui

xsessiondir = $(datadir)/xsessions
xsession_in_files = mate.desktop.in
xsession_DATA = $(xsession_in_files:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_in_files = mate-session-properties.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

if USE_MATE_WM
desktop_in_files += mate-wm.desktop.in
endif

@INTLTOOL_DESKTOP_RULE@

gsettingsschema_in_files = org.mate.session.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

EXTRA_DIST =					\
	mate-wm				\
	$(xsession_in_files)			\
	$(gsettingsschema_in_files)		\
	$(ui_DATA)				\
	$(pixmap_DATA_dist)

CLEANFILES =					\
	$(xsession_DATA)			\
	$(desktop_DATA)				\
	$(gsettings_SCHEMAS)			\
	mate-wm.desktop

DISTCLEANFILES =				\
	$(desktop_in_files)

-include $(top_srcdir)/git.mk
