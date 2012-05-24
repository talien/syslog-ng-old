moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
module_LTLIBRARIES = libredisdd.la

export top_srcdir

if ENABLE_REDIS

libredisdd_la_CFLAGS = $(hiredis_CFLAGS)
libredisdd_la_SOURCES = redisdd-grammar.y redisdd.c redisdd.h redisdd-parser.c redisdd-parser.h
libredisdd_la_LIBADD = $(MODULE_DEPS_LIBS) $(hiredis_LIBS)
libredisdd_la_LDFLAGS = $(MODULE_LDFLAGS)

endif

BUILT_SOURCES = redisdd-grammar.y redisdd-grammar.c redisdd-grammar.h
EXTRA_DIST = $(BUILT_SOURCES) redisdd-grammar.ym

include $(top_srcdir)/build/lex-rules.am
