#ifndef _LUA_CONFIG_H
#define _LUA_CONFIG_H

#include "cfg.h"
#include "logsource.h"
#include "logreader.h"

#define LUA_SOURCE_DRIVER_TYPE "SyslogNG.SourceDriver"
#define LUA_DESTINATION_DRIVER_TYPE "SyslogNG.DestinationDriver"
#define LUA_LOG_EXPR_TYPE "SyslogNG.LogExpr"
#define LUA_LOG_FORK_TYPE "SyslogNG.LogFork"

typedef void (*cfg_lua_register_func)(GlobalConfig* conf);

int lua_config_load(GlobalConfig* conf, const char* filename);
int lua_create_userdata_from_pointer(lua_State* state, void* data, const char* type);
int lua_parse_source_options(lua_State* state,  LogSourceOptions* s);
int lua_parse_reader_options(lua_State* state,  LogReaderOptions* s);
void* lua_check_and_convert_userdata(lua_State* state, int index, const char* type);
#endif
