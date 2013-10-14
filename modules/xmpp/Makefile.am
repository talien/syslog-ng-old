DIST_SUBDIRS= modules/xmpp/iksemel/

if ENABLE_XMPP

module_LTLIBRARIES += modules/xmpp/libxmpp.la
modules_xmpp_libxmpp_la_CFLAGS = \
    -I$(top_srcdir)/modules/xmpp    \
    -I$(top_builddir)/modules/xmpp  \
    $(LIBIKSEMEL_CFLAGS)

modules_xmpp_libxmpp_la_DEPENDENCIES  = \
    $(MODULE_DEPS_LIBS)

if IKSEMEL_INTERNAL
modules_xmpp_libxmpp_la_DEPENDENCIES += modules/xmpp/iksemel/libiksemel.la

modules/xmpp/iksemel/libiksemel.la:
	${MAKE} -C modules/xmpp/iksemel
else
endif

modules_xmpp_libxmpp_la_SOURCES = \
    modules/xmpp/xmpp-grammar.y \
    modules/xmpp/xmpp.c \
    modules/xmpp/xmpp.h \
    modules/xmpp/xmpp-parser.c \
    modules/xmpp/xmpp-parser.h \
    modules/xmpp/xmpp-lib.c \
    modules/xmpp/xmpp-lib.h
modules_xmpp_libxmpp_la_LIBADD = \
    $(LIBIKSEMEL_LIBS) $(MODULE_DEPS_LIBS) -liksemel $(OPENSSL_LIBS)
modules_xmpp_libxmpp_la_LDFLAGS = \
    $(MODULE_LDFLAGS) -liksemel

modules/xmpp modules/xmpp/ mod-xmpp: \
    modules/xmpp/libxmpp.la

else
modules/xmpp modules/xmpp/ mod-xmpp:
endif

BUILT_SOURCES += \
    modules/xmpp/xmpp-grammar.y \
    modules/xmpp/xmpp-grammar.c \
    modules/xmpp/xmpp-grammar.h

EXTRA_DIST += \
    modules/xmpp/xmpp-grammar.ym

.PHONY: modules/xmpp/ mod-xmpp

#include modules/xmpp/tests/Makefile.am
