#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "luaconfig.h"
#include "driver.h"
#include "logreader.h"
#include <gmodule.h>
#include "lua-filter.h"
#include "lua-parser.h"
#include "afinter.h"
#include "lua-option-parser.h"
#include "lua-options.h"
#include "aflua-dest.h"
#include "aflua-source.h"

#define LUA_C_CALL(name) static int name(lua_State* state)

GlobalConfig* lua_get_config(lua_State* state)
{
    GlobalConfig* result;
    lua_getglobal(state, "__conf");
    result = lua_topointer(state, -1);
    lua_pop(state, 1);
    return result;
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
   if (!udata)
      return NULL;
   void* data = *((void**)udata);
   return data;
}

void* lua_check_user_data (lua_State *state, int userdata_index, const char* type) 
{
  void* data = lua_touserdata(state, userdata_index);
  if (!data) 
    return NULL;
  if (lua_getmetatable(state, userdata_index)) 
  {  
    lua_getfield(state, LUA_REGISTRYINDEX, type);  
    if (lua_rawequal(state, -1, -2)) 
    {  
      lua_pop(state, 2);  
      return data;
    }
    lua_pop(state, 2);
  }
  return NULL;
}


void* lua_check_and_convert_userdata(lua_State* state, int index, const char* type)
{
   return lua_to_pointer_from_userdata(lua_check_user_data(state, index, type));
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
            msg_error("Driver type is not correct", evt_tag_str("expected_type", driver_type), NULL);
            //TODO: fatal or non-fatal error?
        }
        else
        {
	        content = log_expr_node_append_tail(content, log_expr_node_new_pipe(driver, NULL));
        }
      }
      else
      {
         msg_error("Non-driver type passed to driver array", NULL);
         //TODO: fatal or non-fatal error?
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
   lua_create_userdata_from_pointer(state, source, LUA_LOG_EXPR_TYPE);
   return 1; 
}

static int lua_config_source_reference(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogExprNode* rule = log_expr_node_new_source_reference(name, NULL);
   lua_create_userdata_from_pointer(state, rule, LUA_LOG_EXPR_TYPE);
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
   lua_create_userdata_from_pointer(state, destination, LUA_LOG_EXPR_TYPE);
   return 1; 
}

static int lua_config_destination_reference(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogExprNode* rule = log_expr_node_new_destination_reference(name, NULL);
   lua_create_userdata_from_pointer(state, rule, LUA_LOG_EXPR_TYPE);
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

static int lua_config_parse_log_flags(lua_State* state)
{
   int flags = 0;
   lua_pushnil(state);
   while (lua_next(state, -2))
   {
      const char* flag_name = lua_tostring(state, -1);
      flags = flags | log_expr_node_lookup_flag(flag_name);
      lua_pop(state, 1);  
   }
   return flags;
}

LogExprNode* lua_parse_expr_array(lua_State* state, int* flags)
{
   LogExprNode *content = NULL, *item = NULL, *forks = NULL;
   int log_flags = 0;
   //TODO: check type of parameter, must be table!
   lua_pushnil(state);
   while(lua_next(state, -2)) 
   { 
      if(!lua_isnumber(state, -2) && lua_isstring(state, -2))
      {
         char* key = lua_tostring(state, -2);
         if (!strncmp("flags", key, 5))
         {
            log_flags = lua_config_parse_log_flags(state);
         }
      }
      if(lua_isuserdata(state, -1)) 
      {
          item = lua_check_and_convert_userdata(state, -1, LUA_LOG_EXPR_TYPE );
          if (!item)
          {
             item = lua_check_and_convert_userdata(state, -1, LUA_LOG_FORK_TYPE );
             forks = log_expr_node_append_tail(forks, item);
            
          }
          else
	        content = log_expr_node_append_tail(content, item);
      }
      lua_pop(state, 1);
   }
   if (forks)
     content = log_expr_node_append_tail(content,log_expr_node_new_junction(forks, NULL));
   *flags = log_flags;
   return content; 
}

static LogExprNode* lua_config_create_rule_from_exprs(lua_State* state)
{
   int flags = 0;
   LogExprNode* rule = lua_parse_expr_array(state, &flags);
   return log_expr_node_new_log(rule, flags, NULL);
}

static int lua_config_log(lua_State* state)
{
   GlobalConfig* self;
   LogExprNode* rule = lua_config_create_rule_from_exprs(state);
   self = lua_get_config(state);
   cfg_tree_add_object(&self->tree, rule);
   return 0;
}

static int lua_config_embedded_log(lua_State* state)
{
   LogExprNode* rule = lua_config_create_rule_from_exprs(state);
   lua_create_userdata_from_pointer(state, rule, LUA_LOG_FORK_TYPE);
   return 1;
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

static int lua_config_global_options(lua_State* state)
{
   LuaOptionParser* parser;
   GlobalConfig* conf = lua_get_config(state); 
   parser = lua_option_parser_new();
   lua_config_register_global_options(parser, conf); 
   lua_option_parser_parse(parser, state);
   lua_option_parser_destroy(parser);
   return 0;
}

void lua_config_register(lua_State* state, GlobalConfig* conf)
{
   lua_register(state, "Source", lua_config_source);
   lua_register(state, "Destination", lua_config_destination);
   lua_register(state, "Log", lua_config_log);
   lua_register(state, "EmbeddedLog", lua_config_embedded_log);
   lua_register(state, "Internal", lua_config_internal);
   lua_register(state, "Options", lua_config_global_options);
   aflua_register_lua_dest(state);
   aflua_register_lua_source(state);
   lua_register_type(state, LUA_SOURCE_DRIVER_TYPE);
   lua_register_type(state, LUA_DESTINATION_DRIVER_TYPE);
   lua_register_type(state, LUA_LOG_EXPR_TYPE);
   lua_register_type(state, LUA_LOG_FORK_TYPE);
   cfg_lua_register_filter(state);
   cfg_lua_register_parser(state);
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
	     msg_error("Error parsing configuration", evt_tag_str("error",lua_tostring(self->lua_cfg_state,-1)),NULL );
             lua_close(self->lua_cfg_state);
             self->lua_cfg_state = NULL;
	     return FALSE;
	}
    lua_close(self->lua_cfg_state);
    self->lua_cfg_state = NULL;
    return TRUE;
}
