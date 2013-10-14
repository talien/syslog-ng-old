/*
 * Copyright (c) 2012 Nagy, Attila <bra@fsn.hu>
 * Copyright (c) 2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013 Viktor Tusa <tusa@balabit.hu>
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

#include "xmpp.h"
#include "cfg-parser.h"
#include "xmpp-grammar.h"

extern int xmpp_debug;
int xmpp_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword xmpp_keywords[] = {
  { "xmpp",			KW_XMPP },
  { "host",			KW_HOST },
  { "port",			KW_PORT },
  { "user",     KW_USER },
  { "password", KW_PASSWORD },
  { "msg",      KW_MSG},
  { "to",       KW_TO},
  { NULL }
};

CfgParser xmpp_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &xmpp_debug,
#endif
  .name = "xmpp",
  .keywords = xmpp_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) xmpp_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(xmpp_, LogDriver **)
