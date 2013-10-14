#include "luaconfig.h"
#include "lua-filter.h"
#include "filter.h"

#define LUA_FILTER_TYPE "SyslogNG.Filter"

static int lua_config_filter_with_handle(lua_State* state, NVHandle handle)
{
   const char* regexp = lua_tostring(state, 1);
   FilterExprNode* expr = filter_re_new(handle);
   filter_re_set_regexp(expr, regexp);
   lua_create_userdata_from_pointer(state, expr, LUA_FILTER_TYPE);
   return 1;
}

static int lua_config_program_filter(lua_State* state)
{
   return lua_config_filter_with_handle(state, LM_V_PROGRAM);
}

static int lua_config_message_filter(lua_State* state)
{
   return lua_config_filter_with_handle(state, LM_V_MESSAGE);
}

static int lua_config_host_filter(lua_State* state)
{
   return lua_config_filter_with_handle(state, LM_V_HOST);
}

static int lua_config_filter_reference(lua_State* state)
{
   const char* name = lua_get_field_from_table(state, -1, "name");;
   LogExprNode* rule = log_expr_node_new_filter_reference(name, NULL);
   lua_create_userdata_from_pointer(state, rule, LUA_LOG_EXPR_TYPE);
   return 1;
}

static int lua_config_filter_named(lua_State* state)
{
   GlobalConfig* self;
   char* name = lua_get_field_from_table(state, -1, "name");
   lua_getfield(state, -1, "items");
   FilterExprNode* filter = lua_check_and_convert_userdata(state, -1, LUA_FILTER_TYPE);
   LogExprNode* content = log_expr_node_new_pipe(log_filter_pipe_new(filter), NULL);
   LogExprNode* rule = log_expr_node_new_filter(name, content, NULL);
   self = lua_get_config(state);
   cfg_tree_add_object(&self->tree, rule);
   lua_pop(state, 1); 
}

LuaConfigClass filter_class = {
   .name = "filter",
   .instantiate_func = lua_config_filter_named,
   .reference_func = lua_config_filter_reference,
};

static int lua_config_filter(lua_State* state)
{
   return lua_config_object(state, "filter");
}

void cfg_lua_register_filter(lua_State *state)
{
   lua_register_type(state, LUA_FILTER_TYPE);
   lua_config_register_config_class(&filter_class);
   lua_register(state, "Filter", lua_config_filter);
   lua_register(state, "ProgramFilter", lua_config_program_filter);
   lua_register(state, "HostFilter", lua_config_host_filter);
   lua_register(state, "MessageFilter", lua_config_message_filter);
}
