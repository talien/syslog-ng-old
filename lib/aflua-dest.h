#ifndef _AFLUA_DEST_H
#define _AFLUA_DEST_H

#include "driver.h"
#include "logwriter.h"
#include <lua.h>

typedef struct _AFLuaDestDriver
{
  LogDestDriver super;
  lua_State* state;
  const char* function_name;
  LogTemplate* template;
} AFLuaDestDriver;

void aflua_register_lua_dest(lua_State* state);
#endif
