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

int anonym_id_counter = 0;

typedef void (*instantiate_func)(lua_State* state);

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

char* lua_get_field_from_table(lua_State* state, int index, const char* field) 
{
   char* value;
   lua_getfield(state, index, field);
   value = g_strdup(lua_tostring(state, -1));
   lua_pop(state, 1);
   return value;
};

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

LogDriver* lua_create_driver_from_table(lua_State* state, int table_index)
{
   const char* driver_type;
   const char* hidden_func_name;
   driver_type = lua_get_field_from_table(state, -1, "type");
   hidden_func_name = lua_get_field_from_table(state, -1, "real_name");
   lua_getfield(state, -1, "params");
   int top = lua_gettop(state);
   int params_num = lua_objlen(state, -1);
   int i;
   lua_getglobal(state,hidden_func_name);
   for (i =1; i<= params_num; i++)
   {
     lua_pushnumber(state, i);
     lua_gettable(state, top);
   }
   if (lua_pcall(state, params_num, 1, 0))
   {
      printf("Error happened!\n");
      printf("Error: %s\n", lua_tostring(state, -1));
   }
   LogDriver* result = lua_check_and_convert_userdata(state, -1, driver_type);
   lua_pop(state, 2);
   g_free(driver_type);
   g_free(hidden_func_name);
   return result;
 }

LogExprNode* lua_parse_driver_array(lua_State* state, const char* driver_type)
{
   LogExprNode* content = NULL;
   LogDriver* driver = NULL; 
   lua_pushnil(state);
   while(lua_next(state, -2)) 
   { 
      if (lua_istable(state, -1))
      {
        driver = lua_create_driver_from_table(state, -1);
	    if (!driver)
        {
            msg_error("Driver type is not correct", evt_tag_str("expected_type", driver_type), NULL);
            //TODO: fatal or non-fatal error?
        }
        else
        {
	        content = log_expr_node_append_tail(content, log_expr_node_new_pipe((LogPipe*)driver, NULL));
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

static void lua_config_get_name_for_ref(char* buffer, int buf_size, char* type)
{
   snprintf(buffer, buf_size, "%s-ref", type);
};

static void lua_config_generate_name_for_anonym(char* buffer)
{
   sprintf(buffer, "anon#%d", anonym_id_counter);
   anonym_id_counter++;
}

static gboolean lua_config_is_object_instantiated(lua_State* state)
{
   const char* name;
   lua_getglobal(state, "__objects");
   lua_getfield(state, -2, "name");
   lua_gettable(state, -2);
   gboolean res = lua_isnoneornil(state, -1);
   lua_pop(state, 2);
   return !res;
}

static void lua_config_add_object_to_global_table(lua_State* state, const char* table_name)
{
   lua_getglobal(state, table_name);
   lua_pushvalue(state, -2);
   lua_getfield(state, -1, "name");
   lua_pushvalue(state, -2);
   lua_settable(state, -4);
   lua_pop(state, 2);
}

static void lua_config_set_object_instantiated(lua_State* state)
{
   lua_config_add_object_to_global_table(state, "__objects");
}

static void lua_config_register_object_as_named_object(lua_State* state)
{
   lua_config_add_object_to_global_table(state, "__named_objects");
}

static int lua_config_object_named_new(lua_State* state, const char* type)
{
   lua_newtable(state);
   lua_pushvalue(state, 1);
   lua_setfield(state, -2, "name");
   lua_pushvalue(state, 2);
   lua_setfield(state, -2, "items");
   lua_pushstring(state, type);
   lua_setfield(state, -2, "type");
   lua_config_register_object_as_named_object(state);
   return 1;
}

static int lua_config_object_anonym_new(lua_State* state, const char* type)
{
   char name[128];
   lua_config_generate_name_for_anonym(name);
   lua_newtable(state);
   lua_pushstring(state, name);
   lua_setfield(state, -2, "name");
   lua_pushvalue(state, 1);
   lua_setfield(state, -2, "items");
   lua_pushstring(state, type);
   lua_setfield(state, -2, "type");
   return 1;
}

static int lua_config_object_reference_new(lua_State* state, const char* type)
{
   char typename[128];
   lua_newtable(state);
   lua_pushvalue(state, 1);
   lua_setfield(state, -2, "name");
   lua_config_get_name_for_ref(typename, sizeof(typename), type);
   lua_pushstring(state, typename);
   lua_setfield(state, -2, "type");
   return 1;
}

static int lua_config_source_reference(lua_State* state)
{
   const char* name = lua_get_field_from_table(state, -1, "name");
   LogExprNode* rule = log_expr_node_new_source_reference(name, NULL);
   lua_create_userdata_from_pointer(state, rule, LUA_LOG_EXPR_TYPE);
   return 1;
}

static void lua_config_instantiate_named_source(lua_State* state)
{
   GlobalConfig* self;
   const char* name = lua_get_field_from_table(state, -1, "name");
   lua_getfield(state, -1, "items");
   LogExprNode* content = lua_parse_driver_array(state, LUA_SOURCE_DRIVER_TYPE);
   LogExprNode* source = log_expr_node_new_source(name, content, NULL);
   self = lua_get_config(state);
   cfg_tree_add_object(&self->tree, source);
   lua_pop(state, 1);
}

int lua_config_destination_reference(lua_State* state)
{
   const char* name;
   name = lua_get_field_from_table(state, -1, "name");

   LogExprNode* rule = log_expr_node_new_destination_reference(name, NULL);
   lua_create_userdata_from_pointer(state, rule, LUA_LOG_EXPR_TYPE);
   return 1;
}

static int lua_config_instantiate_named_destination(lua_State* state)
{
   GlobalConfig* self;
   const char* name;
   name = lua_get_field_from_table(state, -1, "name");
   lua_getfield(state, -1, "items");

   LogExprNode* content = lua_parse_driver_array(state, LUA_DESTINATION_DRIVER_TYPE);
   LogExprNode* destination = log_expr_node_new_destination(name, content, NULL);
   self = lua_get_config(state);
   cfg_tree_add_object(&self->tree, destination);
   lua_pop(state, 1);
}

GList* lua_config_classes = NULL;

static void lua_config_reference_on_object(lua_State* state)
{
   const char *type; 
   char refname[128];
   type = lua_get_field_from_table(state, -1, "type");
   GList* item;
   for (item = lua_config_classes; item; item = item->next)
   {
      LuaConfigClass* class = (LuaConfigClass*)item->data;
      lua_config_get_name_for_ref(refname, sizeof(refname), class->name);

      if ( (!strcmp(type,class->name)) || (!strcmp(type, refname)))
      {
         class->reference_func(state);
         break;
      }
   }

   g_free(type);
}  

static int lua_config_process_object_ref(lua_State* state)
{
   lua_config_reference_on_object(state); 
}

static void lua_config_process_named_object(lua_State* state, instantiate_func instantiate)
{
   if (lua_config_is_object_instantiated(state))
   {
      lua_config_reference_on_object(state); 
      return;
   }
   lua_config_set_object_instantiated(state);
   instantiate(state);
   lua_config_reference_on_object(state);
}

int lua_config_object(lua_State* state, const char* type)
{
   if (lua_gettop(state) == 1)
   {
      if (lua_istable(state, 1))
	     return lua_config_object_anonym_new(state, type);
      if (lua_isstring(state, 1))
     	return lua_config_object_reference_new(state, type);
      //TODO: Error handling
   }
   return lua_config_object_named_new(state, type);
}

static int lua_config_source(lua_State* state)
{
   return lua_config_object(state, "source");
}

static int lua_config_destination(lua_State* state)
{
   return lua_config_object(state, "destination");
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

LuaConfigClass lua_config_source_class =
{
   .name = "source",
   .instantiate_func = lua_config_instantiate_named_source,
   .reference_func = lua_config_source_reference,
};

LuaConfigClass lua_config_destination_class =
{
   .name = "destination",
   .instantiate_func = lua_config_instantiate_named_destination,
   .reference_func = lua_config_destination_reference,
};

void lua_config_register_config_class(LuaConfigClass* class)
{
   lua_config_classes = g_list_append(lua_config_classes, class);
};

static int lua_process_expr_array_item(lua_State* state)
{
   const char* type;
   char refname[128];
   type = lua_get_field_from_table(state, -1, "type");
   GList* item;
   for (item = lua_config_classes; item; item = item->next)
   {
      LuaConfigClass* class = (LuaConfigClass*)item->data;
      if (!strcmp(type,class->name))
      {
         lua_config_process_named_object(state, class->instantiate_func);
         break;
      }
      lua_config_get_name_for_ref(refname, sizeof(refname), class->name);
      if (!strcmp(type, refname))
      {
         lua_config_process_object_ref(state); 
         break;
      }
   }
   g_free(type);
}

static int lua_config_instantiate_object(lua_State* state)
{
   const char* type;
   type = lua_get_field_from_table(state, -1, "type");
   GList* item;
   for (item = lua_config_classes; item; item = item->next)
   {
      LuaConfigClass* class = (LuaConfigClass*)item->data;
      if (!strcmp(type,class->name))
      {
         class->instantiate_func(state);
         break;
      }
   }
   g_free(type);
}

static int lua_config_instantiate_leftover_objects(lua_State* state)
{
   lua_getglobal(state, "__named_objects");
   gboolean res = lua_isnoneornil(state, -1);
   res = lua_istable(state, -1);
   lua_pushnil(state);
   while(lua_next(state, -2)) 
   {
     char* key = lua_tostring(state, -2);
     if (key != NULL)
     {
       if (!lua_config_is_object_instantiated(state))
          lua_config_instantiate_object(state);
     }
     lua_pop(state, 1);
   }
 
};

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
            lua_pop(state, 1);
            continue;
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
      if(lua_istable(state, -1))
      {
         lua_process_expr_array_item(state);
         item = lua_check_and_convert_userdata(state, -1, LUA_LOG_EXPR_TYPE );
	     content = log_expr_node_append_tail(content, item);
         lua_pop(state, 1);

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

void lua_config_save_params_into_table(lua_State* state)
{
   int i;
   int maxparams = lua_gettop(state);
   lua_newtable(state);
   for (i = 1; i<=maxparams; i++)
   {
      lua_pushvalue(state, i);
      lua_rawseti(state, -2, i);
   }
}

int lua_create_driver_wrapper(lua_State* state)
{
   lua_config_save_params_into_table(state);
   lua_newtable(state);
   lua_pushstring(state, "params");
   lua_pushvalue(state, -3);
   lua_settable(state, -3);
   lua_pushstring(state, "type");
   lua_pushvalue(state, lua_upvalueindex(2));
   lua_settable(state, -3);
   lua_pushstring(state, "real_name");
   lua_pushvalue(state, lua_upvalueindex(1));
   lua_settable(state, -3);
   return 1;
}

void lua_register_driver(lua_State* state, const char* name, const char* type, lua_CFunction driver_func)
{
   //lua_register(state, name, driver_func);
   //register original func as a hidden func
   char hidden_name[128];
   memset(hidden_name, 0, sizeof(hidden_name));
   hidden_name[0] = '_';
   strncpy(&hidden_name[1], name, sizeof(hidden_name));
   lua_register(state, hidden_name, driver_func);
   /* Create a c closure which knows the hidden func name, and returns a table
      Every time the table is processed a new destination should be created */
   lua_pushstring(state, hidden_name);
   lua_pushstring(state, type);
   lua_pushcclosure(state, lua_create_driver_wrapper, 2);
   lua_setglobal(state, name);
   
}

void lua_config_register(lua_State* state, GlobalConfig* conf)
{
   lua_config_register_config_class(&lua_config_source_class);
   lua_config_register_config_class(&lua_config_destination_class);
   lua_register(state, "Source", lua_config_source);
   lua_register(state, "Destination", lua_config_destination);
   lua_register(state, "Log", lua_config_log);
   lua_register(state, "EmbeddedLog", lua_config_embedded_log);
   lua_register(state, "Options", lua_config_global_options);
   lua_register_driver(state, "Internal", LUA_SOURCE_DRIVER_TYPE, lua_config_internal);
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
   lua_newtable(state);
   lua_setglobal(state, "__objects");
   lua_newtable(state);
   lua_setglobal(state, "__named_objects");
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
    lua_config_instantiate_leftover_objects(self->lua_cfg_state);
    lua_close(self->lua_cfg_state);
    self->lua_cfg_state = NULL;
    return TRUE;
}
