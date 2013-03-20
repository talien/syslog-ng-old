#ifndef _AFLUA_DEST_H
#define _AFLUA_DEST_H

#include "driver.h"
#include "logwriter.h"
#include <lua.h>

typedef struct _AFLuaDestDriver
{
  LogDestDriver super;
  lua_State* state;
  char* template_string;
  LogTemplate* template;
  gboolean is_init_function_valid;
} AFLuaDestDriver;

void aflua_register_lua_dest(lua_State* state);
GString* aflua_get_bytecode_from_parameter(lua_State* state, int index);
void aflua_load_bytecode_into_state(lua_State* state, char* name, GString* bytecode);
#endif
