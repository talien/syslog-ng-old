#include "aflua-source.h"
#include "logsource.h"
#include "mainloop.h"
#include "aflua-dest.h"
#include "lua-msg.h"

typedef struct _LuaReader
{
   LogSource super;
   lua_State* state;
   struct iv_task reader_start;
   GThread *thread;
   GMutex *thread_mutex;
   GCond* sleep_condition;
   GCond* wakeup_condition;
   GMutex* sleep_condition_mutex;
   gboolean restart_on_error;
   gboolean exit;
} LuaReader;

void aflua_sd_queue(LogPipe* pipe, LogMessage *msg, LogPathOptions *path_options, gpointer user_data)
{
   log_pipe_forward_msg(pipe, msg, path_options);
   return;
}

void lua_source_wait_till_window_free(LuaReader* self)
{
   GTimeVal tv;
   while (!log_source_free_to_send(&self->super))
   {
     g_get_current_time (&tv);
     g_time_val_add (&tv, 10);
     g_mutex_lock(self->thread_mutex);
     g_cond_timed_wait(self->wakeup_condition, self->thread_mutex, &tv);
     g_mutex_unlock(self->thread_mutex);
   }
} 

static int lua_post(lua_State* state)
{
   LuaReader* self = NULL;
   if (!lua_islightuserdata(state, 1))
   {
      msg_error("lua_post: First parameter is not the parameter of the main source function!", NULL);
      return 0;
   }   
   self = lua_topointer(state, 1);
   LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
   LogMessage* lm = NULL;
   if (lua_isstring(state, 2))
   {
      const char* msg = g_strdup(lua_tostring(state, 2));
      lm = log_msg_new_internal(0, msg);
   }
   else
   {
      lm = lua_msg_to_msg(state, 2);
   }
   if (!lm )
   {
      msg_error("lua_post: Message is null, not posting!", NULL);
      return 0;
   }
   lua_source_wait_till_window_free(self);
   log_msg_refcache_start_producer(lm);
   log_pipe_queue(&self->super.super, lm, &path_options);
   log_msg_refcache_stop();
   if (self->exit)
     return luaL_error(state, "Lua Source is ending");
   return 0;
}

static int lua_reader_sleep(lua_State* state)
{
   LuaReader* self = NULL;
   GTimeVal tv;
   if (!lua_islightuserdata(state, 1))
   {
      msg_error("lua_reader_sleep: First parameter is not the parameter of the main source function!", NULL);
      return 0;
   }   
   self = lua_topointer(state, 1);
   int secs = lua_tointeger(state, 2);
   /*TODO: exchange sleep with a g_cond_wait.
           _deinit should signal this cond.
   */
   //sleep(secs);
   g_get_current_time (&tv);
   g_time_val_add (&tv, secs * 1000 * 1000);
   g_mutex_lock(self->sleep_condition_mutex);
   g_cond_timed_wait(self->sleep_condition, self->sleep_condition_mutex, &tv);
   g_mutex_unlock(self->sleep_condition_mutex);

   if (self->exit)
     return luaL_error(state, "Lua Source is ending");
   return 0;
}

static void lua_reader_thread(void* data)
{  
   LuaReader* self = (LuaReader*) data;
   while (!self->exit)
   {
     lua_getglobal(self->state, "thread_func");
     lua_pushlightuserdata(self->state, self);
     if (lua_pcall(self->state, 1, 0, 0))
     {
       const char* errmsg = lua_tostring(self->state, -1);
       if(!self->exit)
          fprintf(stderr, "Error happened in lua_source, %s\n", errmsg);
     }
     if (!self->restart_on_error)
       break;
   }
   //main_loop_call(main_loop_external_thread_quit, NULL, TRUE);
}

static void lua_start_thread(LuaReader* self)
{
  msg_debug("Starting LuaReader thread", NULL);
  self->thread_mutex = g_mutex_new();
  self->wakeup_condition = g_cond_new();
  self->thread = create_worker_thread(lua_reader_thread, self, TRUE, NULL);
  //TODO: implemented external thread started
}

gboolean lua_reader_init(LogPipe* pipe)
{
   LuaReader* self = (LuaReader*) pipe;
   msg_debug("Initializing LuaReader", NULL);
   iv_task_register(&self->reader_start);
   return TRUE;
}

void lua_reader_signal_sleep_condition(LuaReader* self)
{
  g_mutex_lock(self->sleep_condition_mutex);
  g_cond_signal(self->sleep_condition);
  g_mutex_unlock(self->sleep_condition_mutex);
}

gboolean lua_reader_deinit(LogPipe* pipe)
{
  LuaReader* self = (LuaReader*) pipe;
  self->exit = TRUE;
  lua_reader_signal_sleep_condition(self);
  g_thread_join(self->thread);
  lua_close(self->state);
  return TRUE;
}

void lua_reader_free(LogPipe* pipe)
{
   LuaReader* self = (LuaReader*) pipe;
   g_cond_free(self->sleep_condition);
   g_mutex_free(self->sleep_condition_mutex);
   g_mutex_free(self->thread_mutex);
   g_cond_free(self->wakeup_condition);
}

static void lua_reader_signal_cond(LuaReader* self)
{
  g_mutex_lock(self->thread_mutex);
  g_cond_signal(self->wakeup_condition);
  g_mutex_unlock(self->thread_mutex);
}

static void lua_reader_wakeup(LogSource* s)
{
   LuaReader* self = (LuaReader*) s;
   lua_reader_signal_cond(self);
}

LogPipe* lua_reader_new(GString* func, LogSourceOptions* options, gboolean restart_on_error)
{
   LuaReader* self = g_new0(LuaReader, 1);
   log_source_init_instance(&self->super);
   log_source_set_options(&self->super, options, 0, SCS_INTERNAL, NULL, NULL, FALSE);
   self->super.super.init = lua_reader_init;
   self->super.super.deinit = lua_reader_deinit;
   self->super.super.free_fn = lua_reader_free;
   self->super.wakeup = lua_reader_wakeup;
   IV_TASK_INIT(&self->reader_start);
   self->reader_start.cookie = self;
   self->reader_start.handler = lua_start_thread;
   self->restart_on_error = restart_on_error;
   self->exit = FALSE;
   self->state = lua_open();
   luaL_openlibs(self->state);
   lua_register(self->state, "post", lua_post);
   lua_register(self->state, "sleep", lua_reader_sleep);
   lua_register_message(self->state);
   aflua_load_bytecode_into_state(self->state, "thread_func", func);
   self->sleep_condition = g_cond_new();
   self->sleep_condition_mutex = g_mutex_new();
   return self;
}

gboolean aflua_sd_deinit(LogPipe* s)
{
   AFLuaSourceDriver* self = (AFLuaSourceDriver*) s;
   log_pipe_deinit(self->reader);
   return TRUE;
}

void aflua_sd_free(LogPipe* s)
{
  
}

gboolean aflua_sd_init(LogPipe* s)
{
   AFLuaSourceDriver* self = (AFLuaSourceDriver*) s;
   GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);
   log_source_options_init(&self->source_options, cfg, self->super.super.group);
   log_source_options_defaults(&self->source_options);
   self->reader = lua_reader_new(self->thread_func, &self->source_options, self->restart_on_error);
   log_pipe_append(self->reader, &self->super.super.super);
   log_pipe_init(self->reader, cfg);
   return TRUE;
}


LogDriver* aflua_sd_new(GString* function, gboolean restart_on_error)
{
   msg_debug("Creating Lua Source driver", NULL);
   AFLuaSourceDriver* self = g_new0(AFLuaSourceDriver,1);
   self->thread_func = function;
   self->restart_on_error = restart_on_error;
   log_src_driver_init_instance(&self->super);
   self->super.super.super.init = aflua_sd_init;
   self->super.super.super.deinit = aflua_sd_deinit;
   self->super.super.super.queue = aflua_sd_queue;
   self->super.super.super.free_fn = aflua_sd_free;
   return &self->super.super;
}

void aflua_source_driver(lua_State* state)
{
   GString* byte_code;
   byte_code = aflua_get_bytecode_from_parameter(state, 1);
   gboolean restart_on_error = lua_toboolean(state, 2);
   LogDriver* d = aflua_sd_new(byte_code, restart_on_error);
   lua_create_userdata_from_pointer(state, d, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

void aflua_register_lua_source(lua_State* state)
{
  lua_register_driver(state, "LuaSource", LUA_SOURCE_DRIVER_TYPE, aflua_source_driver);
}

