#ifndef AFSTOMP_PARSER_H_INCLUDED
#define AFSTOMP_PARSER_H_INCLUDED

#include "cfg-parser.h"
#include "cfg-lexer.h"
#include "xmpp.h"

extern CfgParser xmpp_parser;

CFG_PARSER_DECLARE_LEXER_BINDING(xmpp_, LogDriver **) 

#endif
