#include "lua-msg.h"
#include "logmsg.h"
#include <lauxlib.h>
#include <lualib.h> 

#define LUA_MESSAGE_TYPE "SyslogNG.Message"

int lua_message_new(lua_State* state)
{
   LogMessage* self = log_msg_new_empty();
   self->pri = 0;
   self->flags |= LF_LOCAL;
   return lua_create_userdata_from_pointer(state, self, LUA_MESSAGE_TYPE);
}

int lua_message_new_index(lua_State* state)
{
   gssize value_len = 0;
   LogMessage* m = lua_check_and_convert_userdata(state, 1, LUA_MESSAGE_TYPE);
   char* key = lua_tostring(state, 2);
   char* value = lua_tolstring(state, 3, &value_len);
   NVHandle nv_handle = log_msg_get_value_handle(key);
   log_msg_set_value(m, nv_handle, value, value_len);
   return 0;
}

int lua_message_index(lua_State* state)
{ 
   int value_len;
   LogMessage* m = lua_check_and_convert_userdata(state, 1, LUA_MESSAGE_TYPE);
   const char* key = lua_tostring(state, 2);
   NVHandle handle = log_msg_get_value_handle(key);
   const char* value = log_msg_get_value(m, handle, &value_len);
   fprintf(stderr, "Getting value, key:%s, value:%s\n", key, value);
   lua_pushlstring(state, value, value_len);
   return 1;
}

//TODO: do we need GC function?

LogMessage* lua_msg_to_msg(lua_State* state, int index)
{  
  return lua_check_and_convert_userdata(state, index, LUA_MESSAGE_TYPE);
}

static const struct luaL_reg msg_function [] = {
      {"new", lua_message_new},
      {NULL, NULL}
    };
    
int lua_register_message(lua_State* state)
{
   luaL_newmetatable(state, LUA_MESSAGE_TYPE);
    
   lua_pushstring(state, "__newindex");
   lua_pushcfunction(state, lua_message_new_index);  
   lua_settable(state, -3);
   lua_pushstring(state, "__index");
   lua_pushcfunction(state, lua_message_index);  
   lua_settable(state, -3); 
    
   lua_pop(state, 1);
   luaL_openlib(state, "Message", msg_function, 0);

   return 0;   
}
