#include "afsocket-lua.h"
#include <lua.h>
#include "driver.h"

static int afsocket_unix_stream_source(lua_State* state)
{
   const char* name = lua_tostring(state, 1);
   LogDriver* d = afunix_sd_new(SOCK_STREAM, name);
   lua_pushlightuserdata(state, d);
   return 1;
}

void afsocket_register_lua_config(GlobalConfig* cfg)
{
   lua_State* state = cfg->lua_cfg_state;
   lua_register(state, "UnixStreamSource", afsocket_unix_stream_source); 
}
