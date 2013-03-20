#include "logpipe.h"
#include "driver.h"
#include "luaconfig.h"

typedef struct _AFLuaSourceDriver{
   LogSrcDriver super;
   LogPipe* reader;
   LogSourceOptions source_options;
   GString* thread_func;
} AFLuaSourceDriver;

LogDriver* aflua_sd_new(GString* function);
void aflua_register_lua_source(lua_State* state);
