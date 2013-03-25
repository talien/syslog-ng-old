
#include "luaconfig.h"
#include "logmsg.h"

#ifndef _LUA_MSG_H
#define _LUA_MSG_H

int lua_register_message(lua_State* state);
LogMessage* lua_msg_to_msg(lua_State* state, int index);
#endif
