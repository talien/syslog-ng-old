#include "cfg.h"

typedef void (*cfg_lua_register_func)(GlobalConfig* conf);

int lua_config_load(GlobalConfig* conf, const char* filename);
