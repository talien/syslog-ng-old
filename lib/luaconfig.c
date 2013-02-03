#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "luaconfig.h"
#include "driver.h"
#include <gmodule.h>

static GlobalConfig* lua_get_config(lua_State* state)
{
    lua_getglobal(state, "__conf");
    return lua_topointer(state, -1);
}

LogExprNode* lua_parse_driver_array(lua_State* state)
{
   LogExprNode* content = NULL;
   LogDriver* driver = NULL; 
   lua_pushnil(state);
   while(lua_next(state, -2)) { 
      if(lua_islightuserdata(state, -1)) 
      {
          driver = lua_topointer(state, -1);
	  content = log_expr_node_append_tail(content, log_expr_node_new_pipe(driver, NULL));
      }
      lua_pop(state, 1);
   }
   return content; 
}

static int lua_config_source_anonym(lua_State* state)
{
   LogExprNode* content = lua_parse_driver_array(state);
   LogExprNode* source = log_expr_node_new_source(NULL, content, NULL);
   lua_pushlightuserdata(state, source);
   return 1; 
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
   LogExprNode* content = lua_parse_driver_array(state);
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
   LogExprNode* content = lua_parse_driver_array(state);
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
   LogExprNode* content = lua_parse_driver_array(state);
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
void lua_config_register(lua_State* state, GlobalConfig* conf)
{
   lua_register(state, "Source", lua_config_source);
   lua_register(state, "Destination", lua_config_destination);
   lua_register(state, "Log", lua_config_log);
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
