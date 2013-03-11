#ifndef _LUA_OPTIONS_H
#define _LUA_OPTIONS_H

#include <lua.h>
#include "lua-option-parser.h"
#include "logsource.h"
#include "logreader.h"
#include "logwriter.h"
#include "driver.h"
#include "cfg.h"

int lua_parse_source_options(lua_State* state, LogSourceOptions* s);
int lua_parse_reader_options(lua_State* state, LogReaderOptions* s);
void lua_option_parser_register_reader_options(LuaOptionParser* parser, LogReaderOptions* options);
void lua_option_parser_register_writer_options(LuaOptionParser* parser, LogWriterOptions* options);
void lua_option_parser_register_source_options(LuaOptionParser* parser, LogSourceOptions* options);
void lua_option_parser_register_destination_options(LuaOptionParser* parser, LogDestDriver* options);
void lua_config_register_global_options(LuaOptionParser* parser, GlobalConfig* config);
#endif
