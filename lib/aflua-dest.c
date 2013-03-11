#include "aflua-dest.h"
#include "luaconfig.h"
#include "messages.h"

void aflua_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
   AFLuaDestDriver *self = (AFLuaDestDriver *) s;
   GString* str = g_string_sized_new(0);
   log_template_format(self->template, msg, NULL, 0, 0, NULL, str);
   lua_getglobal(self->state, "lua_dest_func");
   lua_pushlstring(self->state, str->str, str->len);
   lua_pcall(self->state, 1, 0, 0);
   log_dest_driver_queue_method(s, msg, path_options, user_data);
   g_string_free(str, TRUE);
};

static gboolean
aflua_dd_init(LogPipe *s)
{
  GError* error = NULL;
  AFLuaDestDriver *self = (AFLuaDestDriver *) s;
  GlobalConfig* cfg = log_pipe_get_config(s);
  self->template = log_template_new(cfg, "luatemp");
  log_template_compile(self->template, self->template_string,&error);
  if (!log_dest_driver_init_method(s))
    return FALSE;
  return TRUE;
};

static gboolean
aflua_dd_deinit(LogPipe *s)
{
  if (!log_dest_driver_deinit_method(s))
    return FALSE;
  return TRUE;

}

static void
aflua_dd_free(LogPipe *s)
{
  AFLuaDestDriver *self = (AFLuaDestDriver *) s;

  lua_close(self->state);
  log_dest_driver_free(s);
  g_free(self->template_string);
}

static int lua_reader_state = 0;

int lua_chunk_writer(lua_State* state, const void* p, size_t size, void* userdata)
{
   GString* buffer = (GString*)userdata;
   g_string_append_len(buffer, p, size);
   return 0;
}

const char* lua_chunk_reader(lua_State* state, void* data, size_t *size)
{
   GString* buffer = (GString*) data;
   if (lua_reader_state == 1)
   {
      *size = buffer->len;
      lua_reader_state = 2;
      return buffer->str;
   }
   else
      return NULL;
}

GString* aflua_get_function_bytecode_from_stack(lua_State* state, int index)
{
   GString* buffer = g_string_sized_new(0);
   lua_pushvalue(state, index);
   lua_dump(state, lua_chunk_writer, buffer);
   lua_pop(state, 1);
   return buffer;
}

GString* aflua_get_function_bytecode_from_name(lua_State* state, char* lua_func_name)
{
   lua_getglobal(state, lua_func_name);
   if (!lua_isfunction(state, -1))
   {
      msg_error("Function not found!", evt_tag_str("func",lua_func_name), NULL);
      return NULL;
   }
   return aflua_get_function_bytecode_from_stack(state, -1);
}

LogDriver* 
aflua_dd_new(lua_State* state, GString* func_byte_code, char* template_string)
{
   AFLuaDestDriver *self = g_new0(AFLuaDestDriver, 1);
   lua_reader_state = 1;
   self->state = lua_open();
   luaL_openlibs(self->state);
   lua_load(self->state, lua_chunk_reader, func_byte_code, "lua_dest_func");
   lua_setglobal(self->state, "lua_dest_func");
   g_string_free(func_byte_code, TRUE);
   self->template_string = template_string;
   log_dest_driver_init_instance(&self->super);
   self->super.super.super.init = aflua_dd_init;
   self->super.super.super.deinit = aflua_dd_deinit;
   self->super.super.super.free_fn = aflua_dd_free;
   self->super.super.super.queue = aflua_dd_queue;
   return &self->super.super;
}

static int
aflua_dest_driver(lua_State* state)
{
   GString* byte_code;
   if (lua_isstring(state, 1))
   {
     const char* name = g_strdup(lua_tostring(state, 1));
     byte_code = aflua_get_function_bytecode_from_name(state, name);
   }
   
   if (lua_isfunction(state, 1))   
   {
     byte_code = aflua_get_function_bytecode_from_stack(state, 1);
   }
   const char* template_string = g_strdup(lua_tostring(state, 2));
   LogDriver* d = aflua_dd_new(state, byte_code, template_string);
   lua_create_userdata_from_pointer(state, d, LUA_DESTINATION_DRIVER_TYPE);
   return 1;
}

void aflua_register_lua_dest(lua_State* state)
{
  lua_register(state, "LuaDest", aflua_dest_driver);
}

