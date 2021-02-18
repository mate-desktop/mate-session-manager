SUBDIRS = icons

bin_SCRIPTS = mate-wm

uidir = $(pkgdatadir)
ui_DATA = \
	session-properties.ui	\
	gsm-inhibit-dialog.ui

hwcompatdir = $(pkgdatadir)
hwcompat_DATA = hardware-compatibility

xsessiondir = $(datadir)/xsessions
xsession_in_files = mate.desktop.in
xsession_DATA = $(xsession_in_files:.desktop.in=.desktop)

desktopdir = $(datadir)/applications
desktop_in_files = mate-session-properties.desktop.in mate-wm.desktop.in
desktop_DATA = mate-session-properties.desktop

if USE_MATE_WM
desktop_DATA += mate-wm.desktop
endif

%.desktop: %.desktop.in Makefile
if USE_NLS
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
else
	$(AM_V_GEN) sed '/^# Translators/d' < $< > $@
endif

gsettingsschema_in_files = org.mate.session.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
@GSETTINGS_RULES@

EXTRA_DIST =					\
	mate-wm				\
	$(xsession_in_files)			\
	$(gsettingsschema_in_files)		\
	$(hwcompat_DATA)			\
	$(ui_DATA)				\
	$(desktop_in_files)			\
	$(pixmap_DATA_dist)

CLEANFILES =					\
	$(xsession_DATA)			\
	$(desktop_DATA)				\
	$(gsettings_SCHEMAS)

-include $(top_srcdir)/git.mk
