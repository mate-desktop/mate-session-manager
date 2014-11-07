XSLTPROC_FLAGS = \
	--nonet \
	--stringparam man.output.quietly 1 \
	--stringparam funcsynopsis.style ansi \
	--stringparam man.th.extra1.suppress 1 \
	--stringparam man.authors.section.enabled 0 \
	--stringparam man.copyright.section.enabled 0

.xml.1:
	$(AM_V_GEN) $(XSLTPROC) $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

man_MANS =				\
	mate-session.1			\
	mate-session-properties.1	\
	mate-session-save.1		\
	mate-wm.1			\
	mate-session-inhibit.1

EXTRA_DIST =				\
	mate-session-inhibit.xml	\
	$(man_MANS)

CLEANFILES =				\
	mate-session-inhibit.1

-include $(top_srcdir)/git.mk
