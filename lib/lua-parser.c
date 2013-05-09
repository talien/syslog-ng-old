#include "lua-parser.h"
#include "logparser.h"

static int lua_config_parser_anonym(lua_State* state)
{
   LogParser* parser = lua_topointer(state, 1); 
   LogExprNode* content = log_expr_node_new_pipe(parser, NULL);
   LogExprNode* rule = log_expr_node_new_parser(NULL, content, NULL);
   lua_create_userdata_from_pointer(state, rule, LUA_LOG_EXPR_TYPE);
   return 1;
}

static int lua_config_parser_reference(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogExprNode* rule = log_expr_node_new_parser_reference(name, NULL);
   lua_create_userdata_from_pointer(state, rule, LUA_LOG_EXPR_TYPE);
   return 1;
}

static int lua_config_parser_named(lua_State* state)
{
   GlobalConfig* self;
   const char* name = lua_tostring(state, 1); 
   LogParser* parser = lua_topointer(state, 2); 
   LogExprNode* content = log_expr_node_new_pipe(parser, NULL);
   LogExprNode* rule = log_expr_node_new_parser(name, content, NULL);
   self = lua_get_config(state);
   cfg_tree_add_object(&self->tree, rule);
   return 0;
}

static int lua_config_parser(lua_State* state)
{
   fprintf(stderr, "Gettop %d\n", lua_gettop(state));
   if (lua_gettop(state) == 1)
   {
      if (lua_islightuserdata(state, 1))
        return lua_config_parser_anonym(state);
      if (lua_isstring(state, 1))
        return lua_config_parser_reference(state);
      //TODO: Error handling
   }
   return lua_config_parser_named(state);

}

void cfg_lua_register_parser(lua_State *state)
{
    lua_register(state, "Parser", lua_config_parser);
}
