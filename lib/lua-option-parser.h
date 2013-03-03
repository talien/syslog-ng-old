#ifndef _LUA_OPTION_PARSER_H
#define _LUA_OPTION_PARSER_H

#include <lua.h>
#include <glib.h>

enum {
  LUA_PARSE_TYPE_INT,
  LUA_PARSE_TYPE_BOOL,
  LUA_PARSE_TYPE_STR,
  LUA_PARSE_TYPE_FUNC,
};

typedef struct _LuaOptionParser
{
  GHashTable* parsers;
} LuaOptionParser;

typedef void (*option_parser_func) (lua_State* state, void* udata);

LuaOptionParser* lua_option_parser_new();
void lua_option_parser_destroy(LuaOptionParser* self);
void lua_option_parser_add(LuaOptionParser* parser, char* key, int type, void* udata);
void lua_option_parser_add_func(LuaOptionParser* parser, char* key, void* udata, option_parser_func func);
void lua_option_parser_parse(LuaOptionParser* parser, lua_State* state);

#endif
