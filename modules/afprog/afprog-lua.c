#include "afprog-lua.h"
#include "afprog.h"
#include "luaconfig.h"

int afprog_lua_program_dd(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogDriver *s = afprogram_dd_new(name);
   lua_create_userdata_from_pointer(state, s, LUA_DESTINATION_DRIVER_TYPE);
   return 1;
}

void afprog_register_lua_config(GlobalConfig* cfg)
{
   lua_State* state = cfg->lua_cfg_state;
   lua_register(state, "ProgramDestination", afprog_lua_program_dd);
};
