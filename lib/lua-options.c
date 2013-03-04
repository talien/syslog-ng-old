#include "lua-options.h"
#include "syslog-names.h"

void lua_source_option_log_prefix_parse(lua_State* state, void* udata)
{
   char* prefix = g_strdup(lua_tostring(state, -1));
   char *p = strrchr(prefix, ':');
   if (p) *p = 0;
   *(char**)udata = g_strdup(prefix);
   g_free(prefix);
}

void lua_option_parser_register_source_options(LuaOptionParser* parser, LogSourceOptions* options)
{
   lua_option_parser_add(parser, "log_iw_size", LUA_PARSE_TYPE_INT, &options->init_window_size);
   lua_option_parser_add(parser, "chain_hostnames", LUA_PARSE_TYPE_BOOL, &options->chain_hostnames);
   lua_option_parser_add(parser, "normalize_hostnames", LUA_PARSE_TYPE_BOOL, &options->normalize_hostnames);
   lua_option_parser_add(parser, "keep_hostname", LUA_PARSE_TYPE_BOOL, &options->keep_hostname);
   lua_option_parser_add(parser, "use_fqdn", LUA_PARSE_TYPE_BOOL, &options->use_fqdn);
   lua_option_parser_add(parser, "program_override", LUA_PARSE_TYPE_STR, &options->program_override);
   lua_option_parser_add(parser, "host_override", LUA_PARSE_TYPE_STR, &options->host_override);
   lua_option_parser_add(parser, "keep_timestamp", LUA_PARSE_TYPE_BOOL, &options->keep_timestamp);
   lua_option_parser_add_func(parser, "log_prefix", &options->program_override, lua_source_option_log_prefix_parse);
   //TODO: tags!
}

void lua_option_parser_register_source_proto_options(LuaOptionParser* parser, LogProtoServerOptions* options)
{
   lua_option_parser_add(parser, "encoding", LUA_PARSE_TYPE_STR, &options->encoding);
   lua_option_parser_add(parser, "log_msg_size", LUA_PARSE_TYPE_INT, &options->max_msg_size);
}

void lua_option_parser_msg_format_set_facility(lua_State* state, void* data)
{
   MsgFormatOptions* options = (MsgFormatOptions*) data;
   char* facility_str = lua_tostring(state, -1);
   int facility = syslog_name_lookup_facility_by_name(facility_str);
   if (options->default_pri == 0xFFFF)
      options->default_pri = LOG_USER;
   options->default_pri = (options->default_pri & ~7) | facility;
}

void lua_option_parser_msg_format_set_severity(lua_State* state, void* data)
{
   MsgFormatOptions* options = (MsgFormatOptions*) data;
   char* severity_str = lua_tostring(state, -1);
   int severity = syslog_name_lookup_level_by_name(severity_str);
   if (options->default_pri == 0xFFFF)
      options->default_pri = LOG_NOTICE;
   options->default_pri = (options->default_pri & 7) | severity;
}

void lua_option_parser_register_msg_format_options(LuaOptionParser* parser, MsgFormatOptions* options)
{
   lua_option_parser_add(parser, "time_zone", LUA_PARSE_TYPE_STR, &options->recv_time_zone);
   lua_option_parser_add_func(parser, "default_facility", options, lua_option_parser_msg_format_set_facility);
   lua_option_parser_add_func(parser, "default_level", options, lua_option_parser_msg_format_set_severity);
}

void lua_reader_options_parse_flags(lua_State* state, void* data)
{
   LogReaderOptions* options = (LogReaderOptions*) data;
   lua_pushnil(state);
   const char* flag;
   while (lua_next(state, -2))
   {
      flag = lua_tostring(state, -1);
      if (!log_reader_options_process_flag(options, flag))
      {
        msg_error("Wrong reader flag", evt_tag_str("flag",flag), NULL);
      }
      lua_pop(state, 1);
   }
}

void lua_option_parser_register_reader_options(LuaOptionParser* parser, LogReaderOptions* options)
{
   lua_option_parser_register_source_options(parser, &options->super);
   lua_option_parser_register_source_proto_options(parser, &options->proto_options.super);
   lua_option_parser_register_msg_format_options(parser, &options->parse_options);
   lua_option_parser_add(parser, "check_hostname", LUA_PARSE_TYPE_BOOL, &options->check_hostname);
   lua_option_parser_add(parser, "log_fetch_limit", LUA_PARSE_TYPE_INT, &options->fetch_limit);
   lua_option_parser_add(parser, "format", LUA_PARSE_TYPE_STR, &options->parse_options.format);
   lua_option_parser_add_func(parser, "flags", options, lua_reader_options_parse_flags);
}

int lua_parse_source_options(lua_State* state, LogSourceOptions* s)
{
   LuaOptionParser* parser = lua_option_parser_new();
   lua_option_parser_register_source_options(parser, s);
   lua_option_parser_parse(parser, state);
   lua_option_parser_destroy(parser);
   return 0;
}

int lua_parse_reader_options(lua_State* state, LogReaderOptions* s)
{
   LuaOptionParser* parser = lua_option_parser_new();
   lua_option_parser_register_reader_options(parser, s);
   lua_option_parser_parse(parser, state);
   lua_option_parser_destroy(parser);
   return 0;
}

static void lua_config_global_option_set_mark(lua_State* state, void* data)
{
   GlobalConfig* config = (GlobalConfig*) data;
   char* mark_mode_name = lua_tostring(state, -1);
   int mark_mode = cfg_lookup_mark_mode(mark_mode_name);
   if ((mark_mode == MM_GLOBAL) || (mark_mode == MM_INTERNAL) )
   {
      config->mark_mode = mark_mode;
   }
   else
   {
      msg_info("WARNING, illegal global mark mode", NULL);
   }
}

static void lua_config_global_option_set_tsformat(lua_State* state, void* data)
{
   GlobalConfig* config = (GlobalConfig*) data;
   char* ts_format = lua_tostring(state, -1);
   config->template_options.ts_format = cfg_ts_format_value(ts_format);
}

static void lua_config_global_option_set_bad_hostname(lua_State* state, void* data)
{
   GlobalConfig* config = (GlobalConfig*) data;
   char* bad_hostname = lua_tostring(state, -1);
   cfg_bad_hostname_set(config, bad_hostname);
}

static void lua_config_global_option_set_use_dns(lua_State* state, void* data)
{
   GlobalConfig* config = (GlobalConfig*) data;
   char* dnsmode = lua_tostring(state, -1);
   if (!strncmp("yes", dnsmode, sizeof(dnsmode)))
     config->use_dns = 1;
   else if (!strncmp("no", dnsmode, sizeof(dnsmode)))
     config->use_dns = 0;
   else if (!strncmp("persist-only", dnsmode, sizeof(dnsmode)))
     config->use_dns = 2;
   else
     msg_error("Wrong parameter for use-dns!", NULL);
}

static void lua_config_global_option_set_owner(lua_State* state, void* data)
{
   GlobalConfig* config = (GlobalConfig*) data;
   const char* owner = lua_tostring(state, -1);
   cfg_file_owner_set(config, owner);
}

static void lua_config_global_option_set_group(lua_State* state, void* data)
{
   GlobalConfig* config = (GlobalConfig*) data;
   const char* group = lua_tostring(state, -1);
   cfg_file_group_set(config, group);
}

static void lua_config_global_option_set_perm(lua_State* state, void* data)
{
   GlobalConfig* config = (GlobalConfig*) data;
   int perm = lua_tointeger(state, -1);
   cfg_file_perm_set(config, perm);
}

static void lua_config_global_option_set_dir_owner(lua_State* state, void* data)
{
   GlobalConfig* config = (GlobalConfig*) data;
   const char* owner = lua_tostring(state, -1);
   cfg_dir_owner_set(config, owner);
}

static void lua_config_global_option_set_dir_group(lua_State* state, void* data)
{
   GlobalConfig* config = (GlobalConfig*) data;
   const char* group = lua_tostring(state, -1);
   cfg_dir_group_set(config, group);
}

static void lua_config_global_option_set_dir_perm(lua_State* state, void* data)
{
   GlobalConfig* config = (GlobalConfig*) data;
   int perm = lua_tointeger(state, -1);
   cfg_dir_perm_set(config, perm);
}

void lua_config_register_global_options(LuaOptionParser* parser, GlobalConfig* config)
{
   lua_option_parser_add(parser, "mark_freq", LUA_PARSE_TYPE_INT, &config->mark_freq);
   lua_option_parser_add(parser, "stats_freq", LUA_PARSE_TYPE_INT, &config->stats_freq);
   lua_option_parser_add(parser, "stats_level", LUA_PARSE_TYPE_INT, &config->stats_level);
   lua_option_parser_add(parser, "flush_lines", LUA_PARSE_TYPE_INT, &config->flush_lines);
   lua_option_parser_add_func(parser, "mark_mode", config, lua_config_global_option_set_mark);
   lua_option_parser_add(parser, "flush_timeout", LUA_PARSE_TYPE_INT, &config->flush_timeout);
   lua_option_parser_add(parser, "chain_hostnames", LUA_PARSE_TYPE_BOOL, &config->chain_hostnames);
   lua_option_parser_add(parser, "normalize_hostnames", LUA_PARSE_TYPE_BOOL, &config->normalize_hostnames);
   lua_option_parser_add(parser, "keep_hostname", LUA_PARSE_TYPE_BOOL, &config->keep_hostname);
   lua_option_parser_add(parser, "check_hostname", LUA_PARSE_TYPE_BOOL, &config->check_hostname);
   lua_option_parser_add_func(parser, "bad_hostname", config, lua_config_global_option_set_bad_hostname);
   lua_option_parser_add(parser, "use_fqdn", LUA_PARSE_TYPE_BOOL, &config->use_fqdn);
   lua_option_parser_add_func(parser, "use_dns", config, lua_config_global_option_set_use_dns);
   lua_option_parser_add(parser, "time_reopen", LUA_PARSE_TYPE_INT, &config->time_reopen);
   lua_option_parser_add(parser, "time_reap", LUA_PARSE_TYPE_INT, &config->time_reap);
   lua_option_parser_add(parser, "suppress",LUA_PARSE_TYPE_INT, &config->suppress);
   lua_option_parser_add(parser, "threaded", LUA_PARSE_TYPE_BOOL, &config->threaded);
   lua_option_parser_add(parser, "log_fifo_size", LUA_PARSE_TYPE_INT, &config->log_fifo_size);
   lua_option_parser_add(parser, "log_msg_size", LUA_PARSE_TYPE_INT, &config->log_msg_size);
   lua_option_parser_add(parser, "keep_timestamp", LUA_PARSE_TYPE_BOOL, &config->keep_timestamp);
   lua_option_parser_add_func(parser, "ts_format", config, lua_config_global_option_set_tsformat);
   lua_option_parser_add(parser, "frac_digits", LUA_PARSE_TYPE_INT, &config->template_options.frac_digits);
   lua_option_parser_add(parser, "create_dirs", LUA_PARSE_TYPE_BOOL, &config->create_dirs);
   lua_option_parser_add_func(parser, "owner", config, lua_config_global_option_set_owner);
   lua_option_parser_add_func(parser, "gruop", config, lua_config_global_option_set_group);
   lua_option_parser_add_func(parser, "perm", config, lua_config_global_option_set_perm);
   lua_option_parser_add_func(parser, "dir_owner", config, lua_config_global_option_set_dir_owner);
   lua_option_parser_add_func(parser, "dir_group", config, lua_config_global_option_set_dir_group);
   lua_option_parser_add_func(parser, "dir_perm", config,  lua_config_global_option_set_dir_perm);
   lua_option_parser_add(parser, "dns_cache", LUA_PARSE_TYPE_BOOL, &config->use_dns_cache);
   lua_option_parser_add(parser, "dns_cache_size", LUA_PARSE_TYPE_INT, &config->dns_cache_size);
   lua_option_parser_add(parser, "dns_cache_expire", LUA_PARSE_TYPE_INT, &config->dns_cache_expire);
   lua_option_parser_add(parser, "dns_cache_expire_failed", LUA_PARSE_TYPE_INT, &config->dns_cache_expire_failed);
   lua_option_parser_add(parser, "dns_cache_hosts", LUA_PARSE_TYPE_STR, &config->dns_cache_hosts);
   lua_option_parser_add(parser, "file_template", LUA_PARSE_TYPE_STR, &config->file_template_name);
   lua_option_parser_add(parser, "proto_template", LUA_PARSE_TYPE_STR, &config->proto_template_name);
   lua_option_parser_add(parser, "recv_time_zone", LUA_PARSE_TYPE_STR, &config->recv_time_zone);
   lua_option_parser_add(parser, "send_time_zone", LUA_PARSE_TYPE_STR, &config->template_options.time_zone[LTZ_SEND]);
   lua_option_parser_add(parser, "local_time_zone", LUA_PARSE_TYPE_STR, &config->template_options.time_zone[LTZ_LOCAL]);
}
