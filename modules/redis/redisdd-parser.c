/*
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "redisdd.h"
#include "cfg-parser.h"
#include "redisdd-grammar.h"

extern int redis_dd_debug;
int redis_dd_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword redis_dd_keywords[] = {
  { "redis",                    KW_REDIS },
  { "host",                     KW_HOST },
  { "port",                     KW_PORT },
  { "prefix",                   KW_PREFIX },
  { "command",                  KW_COMMAND },
  { NULL }
};

CfgParser redis_dd_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &redis_dd_debug,
#endif
  .name = "redisdd",
  .keywords = redis_dd_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) redis_dd_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(redis_dd_, LogDriver **)
