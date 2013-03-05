#include "luaconfig.h"
#include "lua-filter.h"
#include "filter.h"

static int lua_config_program_filter(lua_State* state)
{
   const char* regexp = lua_tostring(state, 1);
   FilterExprNode* expr = filter_re_new(LM_V_PROGRAM);
   filter_re_set_regexp(expr, regexp);
   lua_pushlightuserdata(state, expr);
   return 1;
}

static int lua_config_filter_anonym(lua_State* state)
{
   FilterExprNode* filter = lua_topointer(state, 1);
   LogExprNode* content = log_expr_node_new_pipe(log_filter_pipe_new(filter), NULL);
   LogExprNode* rule = log_expr_node_new_filter(NULL, content, NULL);
   lua_create_userdata_from_pointer(state, rule, LUA_LOG_EXPR_TYPE);
   return 1;
}

static int lua_config_filter_reference(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogExprNode* rule = log_expr_node_new_filter_reference(name, NULL);
   lua_create_userdata_from_pointer(state, rule, LUA_LOG_EXPR_TYPE);
   return 1;
}

static int lua_config_filter_named(lua_State* state)
{
   GlobalConfig* self;
   const char* name = lua_tostring(state, 1);
   FilterExprNode* filter = lua_topointer(state, 2);
   LogExprNode* content = log_expr_node_new_pipe(log_filter_pipe_new(filter), NULL);
   LogExprNode* rule = log_expr_node_new_filter(name, content, NULL);
   self = lua_get_config(state);
   cfg_tree_add_object(&self->tree, filter);
   return 0; 
}

static int lua_config_filter(lua_State* state)
{
   if (lua_gettop(state) == 1)
   {
      if (lua_islightuserdata(state, 1))
        return lua_config_filter_anonym(state);
      if (lua_isstring(state, 1))
        return lua_config_filter_reference(state);
      //TODO: Error handling
   }
   return lua_config_filter_named(state);

}

void cfg_lua_register_filter(lua_State *state)
{
   lua_register(state, "Filter", lua_config_filter);
   lua_register(state, "ProgramFilter", lua_config_program_filter);
}
