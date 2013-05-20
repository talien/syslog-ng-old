moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
module_LTLIBRARIES = librss.la

export top_srcdir

librss_la_SOURCES = rss-grammar.y rss.c rss.h rss-parser.c rss-parser.h
librss_la_LIBADD = ../../lib/libsyslog-ng.la
librss_la_LDFLAGS = -avoid-version -module -no-undefined

BUILT_SOURCES = rss-grammar.y rss-grammar.c rss-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) rss-grammar.ym

include $(top_srcdir)/build/lex-rules.am
