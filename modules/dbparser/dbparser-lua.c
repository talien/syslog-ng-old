#include "dbparser-lua.h"
#include "dbparser.h"
#include "luaconfig.h"

int dbparser_lua_new(lua_State* state)
{
    const char* name = g_strdup(lua_tostring(state, 1));
    LogParser* parser = log_db_parser_new();
    log_db_parser_set_db_file(parser, name);
    lua_pushlightuserdata(state, parser);
    return 1;
}

void dbparser_register_lua_config(GlobalConfig* cfg)
{
    pattern_db_global_init();
    lua_State* state = cfg->lua_cfg_state;
    lua_register(state, "DBParser", dbparser_lua_new);
}
