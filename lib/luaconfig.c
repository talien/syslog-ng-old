#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "luaconfig.h"
#include "driver.h"
#include "logreader.h"
#include <gmodule.h>
#include "lua-filter.h"
#include "afinter.h"

#define LUA_C_CALL(name) static int name(lua_State* state)

GlobalConfig* lua_get_config(lua_State* state)
{
    lua_getglobal(state, "__conf");
    return lua_topointer(state, -1);
}

void lua_register_type(lua_State* state, const char* type)
{
   luaL_newmetatable(state, type);
   lua_pop(state, 1);
}

int lua_create_userdata_from_pointer(lua_State* state, void* data, const char* type)
{
   void* userdata = lua_newuserdata(state, sizeof(data));
   *((void**)userdata) = data;
   luaL_getmetatable(state, type);
   lua_setmetatable(state, -2);   
   return 1;
}

void* lua_to_pointer_from_userdata(void* udata)
{
   void* data = *((void**)udata);
   return data;
}

void* lua_check_and_convert_userdata(lua_State* state, int index, const char* type)
{
   return lua_to_pointer_from_userdata(luaL_checkudata(state, index, type));
}

LogExprNode* lua_parse_driver_array(lua_State* state, const char* driver_type)
{
   LogExprNode* content = NULL;
   LogDriver* driver = NULL; 
   lua_pushnil(state);
   while(lua_next(state, -2)) { 
      if(lua_isuserdata(state, -1)) 
      {
          driver = lua_check_and_convert_userdata(state, -1, driver_type);
	  if (!driver)
	  {
             fprintf(stderr, "Lofasz happened!\n");
	  }
	  else
	  {
	    content = log_expr_node_append_tail(content, log_expr_node_new_pipe(driver, NULL));
          }
      }
      else
      {
          fprintf(stderr, "Lofasz2 happened!\n");
      }
      lua_pop(state, 1);
   }
   return content; 
}

LUA_C_CALL(lua_config_source_anonym)
//static int lua_config_source_anonym(lua_State* state)
{
   LogExprNode* content = lua_parse_driver_array(state, LUA_SOURCE_DRIVER_TYPE);
   LogExprNode* source = log_expr_node_new_source(NULL, content, NULL);
   lua_pushlightuserdata(state, source);
   return 1; 
}

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

void lua_source_option_log_prefix_parse(lua_State* state, void* udata)
{
   char* prefix = g_strdup(lua_tostring(state, -1));
   char *p = strrchr(prefix, ':'); 
   if (p) *p = 0;
   *(char**)udata = g_strdup(prefix);
   g_free(prefix);
}


void lua_option_parser_register_source_options(LuaOptionParser* parser, LogSourceOptions* options)
{
   lua_option_parser_add(parser, "log_iw_size", LUA_PARSE_TYPE_INT, &options->init_window_size);
   lua_option_parser_add(parser, "chain_hostnames", LUA_PARSE_TYPE_BOOL, &options->chain_hostnames);
   lua_option_parser_add(parser, "normalize_hostnames", LUA_PARSE_TYPE_BOOL, &options->normalize_hostnames);
   lua_option_parser_add(parser, "keep_hostname", LUA_PARSE_TYPE_BOOL, &options->keep_hostname);
   lua_option_parser_add(parser, "use_fqdn", LUA_PARSE_TYPE_BOOL, &options->use_fqdn);
   lua_option_parser_add(parser, "program_override", LUA_PARSE_TYPE_STR, &options->program_override);
   lua_option_parser_add(parser, "host_override", LUA_PARSE_TYPE_STR, &options->host_override);
   lua_option_parser_add(parser, "keep_timestamp", LUA_PARSE_TYPE_BOOL, &options->keep_timestamp);
   lua_option_parser_add_func(parser, "log_prefix", &options->program_override, lua_source_option_log_prefix_parse);
   //TODO: tags!
}

void lua_option_parser_register_reader_options(LuaOptionParser* parser, LogReaderOptions* options)
{
   lua_option_parser_register_source_options(parser, &options->super);
   lua_option_parser_add(parser, "check_hostname", LUA_PARSE_TYPE_BOOL, &options->check_hostname);
   lua_option_parser_add(parser, "log_fetch_limit", LUA_PARSE_TYPE_INT, &options->fetch_limit);
   lua_option_parser_add(parser, "format", LUA_PARSE_TYPE_STR, &options->parse_options.format);
}

int lua_parse_source_options(lua_State* state, LogSourceOptions* s)
{
  LuaOptionParser* parser = lua_option_parser_new();
  lua_option_parser_register_source_options(parser, s);
  lua_option_parser_parse(parser, state);
  lua_option_parser_destroy(parser);
  return 0;
}

int lua_parse_reader_options(lua_State* state, LogReaderOptions* s)
{
  LuaOptionParser* parser = lua_option_parser_new();
  lua_option_parser_register_reader_options(parser, s);
  lua_option_parser_parse(parser, state);
  lua_option_parser_destroy(parser);
  return 0;
}

static int lua_config_source_reference(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogExprNode* rule = log_expr_node_new_source_reference(name, NULL);
   lua_pushlightuserdata(state, rule);
   return 1;
}

static int lua_config_source_named(lua_State* state)
{
   GlobalConfig* self;
   const char* name = g_strdup(lua_tostring(state, 1));
   LogExprNode* content = lua_parse_driver_array(state, LUA_SOURCE_DRIVER_TYPE);
   LogExprNode* source = log_expr_node_new_source(name, content, NULL);
   self = lua_get_config(state);
   cfg_tree_add_object(&self->tree, source);
   return 0;
}

static int lua_config_source(lua_State* state)
{
   if (lua_gettop(state) == 1)
   {
      if (lua_istable(state, 1))
	return lua_config_source_anonym(state);
      if (lua_isstring(state, 1))
	return lua_config_source_reference(state);
      //TODO: Error handling
   }
   return lua_config_source_named(state);
}

static int lua_config_destination_anonym(lua_State* state)
{
   LogExprNode* content = lua_parse_driver_array(state, LUA_DESTINATION_DRIVER_TYPE);
   LogExprNode* destination = log_expr_node_new_destination(NULL, content, NULL);
   lua_pushlightuserdata(state, destination);
   return 1; 
}

static int lua_config_destination_reference(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogExprNode* rule = log_expr_node_new_destination_reference(name, NULL);
   lua_pushlightuserdata(state, rule);
   return 1;
}

static int lua_config_destination_named(lua_State* state)
{
   GlobalConfig* self;
   const char* name = g_strdup(lua_tostring(state, 1));
   LogExprNode* content = lua_parse_driver_array(state, LUA_DESTINATION_DRIVER_TYPE);
   LogExprNode* destination = log_expr_node_new_destination(name, content, NULL);
   self = lua_get_config(state);
   cfg_tree_add_object(&self->tree, destination);
   return 0;
}

static int lua_config_destination(lua_State* state)
{
   if (lua_gettop(state) == 1)
   {
      if (lua_istable(state, 1))
	return lua_config_destination_anonym(state);
      if (lua_isstring(state, 1))
	return lua_config_destination_reference(state);
      //TODO: Error handling
   }
   return lua_config_destination_named(state);
}

LogExprNode* lua_parse_expr_array(lua_State* state)
{
   LogExprNode *content = NULL, *item = NULL;
   lua_pushnil(state);
   while(lua_next(state, -2)) { 
      if(lua_islightuserdata(state, -1)) 
      {
          item = lua_topointer(state, -1);
	  content = log_expr_node_append_tail(content, item);
      }
      lua_pop(state, 1);
   }
   return content; 
}

static int lua_config_log(lua_State* state)
{
   GlobalConfig* self;
   LogExprNode* rule = lua_parse_expr_array(state);
   rule = log_expr_node_new_log(rule, 0, NULL);
   self = lua_get_config(state);
   cfg_tree_add_object(&self->tree, rule);
   return 0;
}

static int lua_config_internal(lua_State* state)
{
   AFInterSourceDriver* d = afinter_sd_new();
   msg_debug("Parsing internal source", evt_tag_int("gettop", lua_gettop(state)), NULL);
   if (lua_gettop(state) == 1)
   {
      msg_debug("Parsing internal source options", NULL);
      lua_parse_source_options(state, &d->source_options);
   } 
   lua_create_userdata_from_pointer(state, d, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

void lua_config_register(lua_State* state, GlobalConfig* conf)
{
   lua_register(state, "Source", lua_config_source);
   lua_register(state, "Destination", lua_config_destination);
   lua_register(state, "Log", lua_config_log);
   lua_register(state, "Internal", lua_config_internal);
   lua_register_type(state, LUA_SOURCE_DRIVER_TYPE);
   lua_register_type(state, LUA_DESTINATION_DRIVER_TYPE);
   lua_register_type(state, LUA_LOG_EXPR_TYPE);
   cfg_lua_register_filter(state);
   lua_pushlightuserdata(state, conf);
   lua_setglobal(state, "__conf");
}

int lua_config_load(GlobalConfig* self, const char* filename)
{
    self->lua_cfg_state = lua_open();
    plugin_load_all_modules(self);
    luaL_openlibs(self->lua_cfg_state);
    lua_config_register(self->lua_cfg_state, self);
    if (luaL_loadfile(self->lua_cfg_state, filename) ||  
        lua_pcall(self->lua_cfg_state, 0,0,0) )
	{
	     fprintf(stderr,"%s\n", lua_tostring(self->lua_cfg_state,-1));
             lua_close(self->lua_cfg_state);
             self->lua_cfg_state = NULL;
	     return FALSE;
	}
    lua_close(self->lua_cfg_state);
    self->lua_cfg_state = NULL;
    return TRUE;
}
