#include "lua-parser.h"
#include "logparser.h"

static int lua_config_parser_reference(lua_State* state)
{
   char* name = lua_get_field_from_table(state, -1, "name");
   LogExprNode* rule = log_expr_node_new_parser_reference(name, NULL);
   lua_create_userdata_from_pointer(state, rule, LUA_LOG_EXPR_TYPE);
   return 1;
}

static int lua_config_parser_named(lua_State* state)
{
   GlobalConfig* self;
   const char* name = lua_get_field_from_table(state, -1, "name");
   LogParser* parser = lua_check_and_convert_userdata(state, -1, LUA_PARSER_TYPE); 
   LogExprNode* content = log_expr_node_new_pipe(parser, NULL);
   LogExprNode* rule = log_expr_node_new_parser(name, content, NULL);
   self = lua_get_config(state);
   cfg_tree_add_object(&self->tree, rule);
   lua_pop(state, 1);
}

static int lua_config_parser(lua_State* state)
{
   return lua_config_object(state, "parser");
}

LuaConfigClass parser_class = { 
   .name = "parser",
   .instantiate_func = lua_config_parser_named,
   .reference_func = lua_config_parser_reference,
};


void cfg_lua_register_parser(lua_State *state)
{
    lua_register_type(state, LUA_PARSER_TYPE);
    lua_config_register_config_class(&parser_class);
    lua_register(state, "Parser", lua_config_parser);
}
