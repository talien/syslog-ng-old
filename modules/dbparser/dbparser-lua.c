#include "dbparser-lua.h"
#include "dbparser.h"
#include "luaconfig.h"
#include "lua-parser.h"

int dbparser_lua_new(lua_State* state)
{
    const char* name = g_strdup(lua_tostring(state, 1));
    LogParser* parser = log_db_parser_new();
    log_db_parser_set_db_file(parser, name);
    lua_create_userdata_from_pointer(state, parser, LUA_PARSER_TYPE);
    return 1;
}

void dbparser_register_lua_config(GlobalConfig* cfg)
{
    pattern_db_global_init();
    lua_State* state = cfg->lua_cfg_state;
    lua_register_driver(state, "DBParser", LUA_PARSER_TYPE, dbparser_lua_new);
}
