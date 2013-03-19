#include "affile-dest.h"
#include "affile-source.h"
#include "affile-common.h"
#include <lua.h>
#include "luaconfig.h"
#include "lua-options.h"

static void affile_lua_file_destination_set_create_dirs(lua_State* state, void* data)
{
   AFFileDestDriver* driver = (AFFileDestDriver*) data;
   int create_dirs = lua_toboolean(state, -1);
   affile_dd_set_create_dirs(driver, create_dirs);
}

static void affile_lua_file_destination_set_overwrite_if_older(lua_State* state, void* data)
{
   AFFileDestDriver* driver = (AFFileDestDriver*) data;
   int overwrite_if_older = lua_tointeger(state, -1);
   affile_dd_set_overwrite_if_older(driver, overwrite_if_older);
}

static void affile_lua_file_destination_set_fsync(lua_State* state, void* data)
{
   AFFileDestDriver* driver = (AFFileDestDriver*) data;
   int fsync = lua_toboolean(state, -1);
   affile_dd_set_fsync(driver, fsync);
}

static void affile_lua_file_destination_set_local_time_zone(lua_State* state, void* data)
{
   AFFileDestDriver* driver = (AFFileDestDriver*) data;
   char* local_time_zone = g_strdup(lua_tostring(state, -1));
   affile_dd_set_local_time_zone(driver, local_time_zone);
}

static void affile_lua_register_file_destination_options(LuaOptionParser* parser, LogDriver* d)
{
   lua_option_parser_register_destination_options(parser, d);
   lua_option_parser_register_writer_options(parser, &((AFFileDestDriver *) d)->writer_options);
   lua_option_parser_register_file_perm_options(parser, &((AFFileDestDriver *) d)->file_perm_options);
   lua_option_parser_add(parser, "optional", LUA_PARSE_TYPE_BOOL, &d->optional);
   lua_option_parser_add_func(parser, "create_dirs", d, affile_lua_file_destination_set_create_dirs);
   lua_option_parser_add_func(parser, "overwrite_if_older", d, affile_lua_file_destination_set_overwrite_if_older);
   lua_option_parser_add_func(parser, "fsync", d, affile_lua_file_destination_set_fsync);
   lua_option_parser_add_func(parser, "local_time_zone", d, affile_lua_file_destination_set_local_time_zone);
}

int affile_lua_file_dd(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogDriver *d = affile_dd_new(name, 0);
   lua_config_register_and_parse_options(state, d, affile_lua_register_file_destination_options);
   lua_create_userdata_from_pointer(state, d, LUA_DESTINATION_DRIVER_TYPE);
   return 1;
}

static void affile_lua_register_pipe_destination_options(LuaOptionParser* parser, LogDriver* d)
{
   lua_option_parser_register_destination_options(parser, d);
   lua_option_parser_register_writer_options(parser, &((AFFileDestDriver *) d)->writer_options);
   lua_option_parser_register_file_perm_options(parser, &((AFFileDestDriver *) d)->file_perm_options);
}

int affile_lua_pipe_dd(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogDriver *d = affile_dd_new(name, AFFILE_PIPE);
   lua_config_register_and_parse_options(state, d, affile_lua_register_pipe_destination_options);
   lua_create_userdata_from_pointer(state, d, LUA_DESTINATION_DRIVER_TYPE);
   return 1;
}

void affile_lua_register_file_source_options(LuaOptionParser* parser, AFFileSourceDriver* sd)
{
   lua_option_parser_register_reader_options(parser, &sd->reader_options);
   lua_option_parser_add(parser, "follow_freq", LUA_PARSE_TYPE_INT, &sd->reader_options.follow_freq);
   lua_option_parser_add(parser, "pad_size", LUA_PARSE_TYPE_INT,&sd->pad_size);
}

int affile_lua_file_sd(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogDriver *s = affile_sd_new(name, 0);
   lua_config_register_and_parse_options(state, s, affile_lua_register_file_source_options);
   lua_create_userdata_from_pointer(state, s, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

void affile_lua_register_pipe_source_options(LuaOptionParser* parser, AFFileSourceDriver* sd)
{
   lua_option_parser_register_reader_options(parser, &sd->reader_options);
   lua_option_parser_add(parser, "pad_size", LUA_PARSE_TYPE_INT,&sd->pad_size);
   lua_option_parser_add(parser, "optional", LUA_PARSE_TYPE_BOOL, &((LogDriver*)sd)->optional);
}

int affile_lua_pipe_sd(lua_State* state)
{
   const char* name = g_strdup(lua_tostring(state, 1));
   LogDriver *s = affile_sd_new(name, AFFILE_PIPE);
   lua_config_register_and_parse_options(state, s, affile_lua_register_pipe_source_options);
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
   lua_register(state, "PipeDestination", affile_lua_pipe_dd);
}
