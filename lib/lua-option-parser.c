#include "lua-option-parser.h"
#include "messages.h"

typedef struct _LuaOptionParserItem
{
   int type;
   void* udata;
   option_parser_func func;

} LuaOptionParserItem;

LuaOptionParserItem* lua_option_parser_item_new(int type, void* udata, option_parser_func func)
{
  LuaOptionParserItem* self = g_new0(LuaOptionParserItem, 1);
  self->type = type;
  self->udata = udata;
  self->func = func;
  return self;
}

void lua_option_parser_item_destroy(void* item)
{
  LuaOptionParserItem* self = (LuaOptionParserItem*) item;
  g_free(self);
}

LuaOptionParser* lua_option_parser_new()
{
   LuaOptionParser* self = g_new0(LuaOptionParser, 1);
   self->parsers = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, lua_option_parser_item_destroy);
   return self;
}

void lua_option_parser_destroy(LuaOptionParser* self)
{
   g_hash_table_destroy(self->parsers);
   g_free(self);
}

void lua_option_parser_add(LuaOptionParser* parser, char* key, int type, void* udata)
{
   LuaOptionParserItem * item = lua_option_parser_item_new(type, udata, NULL);
   g_hash_table_insert(parser->parsers, key, item);
}

void lua_option_parser_add_func(LuaOptionParser* parser, char* key, void* udata, option_parser_func func)
{
   LuaOptionParserItem * item = lua_option_parser_item_new(LUA_PARSE_TYPE_FUNC, udata, func);
   g_hash_table_insert(parser->parsers, key, item);
}

void lua_option_parser_parse(LuaOptionParser* parser, lua_State* state)
{
   if (!lua_istable(state, -1))
   {
     msg_error("Expecting table!", NULL);
     return;
   }
   lua_pushnil(state);
   const char* key;
   while(lua_next(state, -2))
   {
      if (lua_isstring(state, -2))
      {
         key = lua_tostring(state, -2);
         msg_debug("Parsing options", evt_tag_str("option", key), NULL);
         LuaOptionParserItem* item = g_hash_table_lookup(parser->parsers, key);
         if (item)
         {
            if (item->type == LUA_PARSE_TYPE_INT)
            {
               *((int*)item->udata) = lua_tointeger(state, -1);
            }

            if (item->type == LUA_PARSE_TYPE_BOOL)
            {
               *((int*)item->udata) = lua_toboolean(state, -1);
            }

            if (item->type == LUA_PARSE_TYPE_STR)
            {
               *((char**)item->udata) = g_strdup(lua_tostring(state, -1));
            }
            if (item->type == LUA_PARSE_TYPE_FUNC)
            {
                item->func(state, item->udata);
            }
        }
        else msg_debug("Unknown key for source options", evt_tag_str("key", key), NULL);
     }
     lua_pop(state, 1);
  }

}
