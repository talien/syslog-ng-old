#ifndef _LUA_PARSER_H
#define _LUA_PARSER_H
#include "luaconfig.h"

#define LUA_PARSER_TYPE "SyslogNG.Parser"

void cfg_lua_register_parser(lua_State *state);
#endif
