#include "afsocket-lua.h"
#include "afunix-source.h"
#include <lua.h>
#include "driver.h"
#include "luaconfig.h"

static int afsocket_unix_stream_source(lua_State* state)
{
   const char* name = lua_tostring(state, 1);
   LogDriver* d = afunix_sd_new(SOCK_STREAM, name);
   lua_parse_reader_options(state, &(((AFUnixSourceDriver*)d)->super.reader_options));
   lua_create_userdata_from_pointer(state, d, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

static int afsocket_unix_stream_destination(lua_State* state)
{
   const char* name = lua_tostring(state, 1);
   LogDriver* d = afunix_dd_new(SOCK_STREAM, name);
   lua_create_userdata_from_pointer(state, d, LUA_DESTINATION_DRIVER_TYPE);
   return 1;
}

void afsocket_register_lua_config(GlobalConfig* cfg)
{
   lua_State* state = cfg->lua_cfg_state;
   lua_register(state, "UnixStreamSource", afsocket_unix_stream_source); 
   lua_register(state, "UnixStreamDestination", afsocket_unix_stream_destination); 
}
