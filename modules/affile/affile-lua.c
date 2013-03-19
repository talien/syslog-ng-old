#include "affile-dest.h"
#include "affile-source.h"
#include "affile-common.h"
#include <lua.h>
#include "luaconfig.h"
#include "lua-options.h"

int affile_lua_file_dd(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogDriver *s = affile_dd_new(name, 0);
   lua_create_userdata_from_pointer(state, s, LUA_DESTINATION_DRIVER_TYPE);
   return 1;
}

int affile_lua_register_file_source_options(LuaOptionParser* parser, AFFileSourceDriver* sd)
{
   lua_option_parser_register_reader_options(parser, &sd->reader_options);
   lua_option_parser_add(parser, "follow_freq", LUA_PARSE_TYPE_INT, &sd->reader_options.follow_freq);
   lua_option_parser_add(parser, "pad_size", LUA_PARSE_TYPE_INT,&sd->pad_size);
}

int affile_lua_parse_file_source_options(lua_State* state, AFFileSourceDriver* sd)
{
   LuaOptionParser* parser = lua_option_parser_new();
   affile_lua_register_file_source_options(parser, sd);
   lua_option_parser_parse(parser, state);
   lua_option_parser_destroy(parser);
   return 0; 
}

int affile_lua_file_sd(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogDriver *s = affile_sd_new(name, 0);
   affile_lua_parse_file_source_options(state, (AFFileSourceDriver*)s);
   ((AFFileSourceDriver*)s)->reader_options.follow_freq = ((AFFileSourceDriver*)s)->reader_options.follow_freq * 1000;
   lua_create_userdata_from_pointer(state, s, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

int affile_lua_register_pipe_source_options(LuaOptionParser* parser, AFFileSourceDriver* sd)
{
   lua_option_parser_register_reader_options(parser, &sd->reader_options);
   lua_option_parser_add(parser, "pad_size", LUA_PARSE_TYPE_INT,&sd->pad_size);
   lua_option_parser_add(parser, "optional", LUA_PARSE_TYPE_BOOL, &((LogDriver*)sd)->optional);
}

int affile_lua_parse_pipe_source_options(lua_State* state, AFFileSourceDriver* sd)
{
   LuaOptionParser* parser = lua_option_parser_new();
   affile_lua_register_pipe_source_options(parser, sd);
   lua_option_parser_parse(parser, state);
   lua_option_parser_destroy(parser);
   return 0; 
}

int affile_lua_pipe_sd(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogDriver *s = affile_sd_new(name, AFFILE_PIPE);
   affile_lua_parse_pipe_source_options(state, (AFFileSourceDriver*)s);
   lua_create_userdata_from_pointer(state, s, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

void affile_register_lua_config(GlobalConfig* cfg)
{
   lua_State* state = cfg->lua_cfg_state;
   msg_debug("Registering affile lua config items", NULL);
   lua_register(state, "FileSource", affile_lua_file_sd);
   lua_register(state, "FileDestination", affile_lua_file_dd);
   lua_register(state, "PipeSource", affile_lua_pipe_sd);
}
