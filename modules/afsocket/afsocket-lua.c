#include "afsocket-lua.h"
#include "afunix-source.h"
#include <lua.h>
#include "driver.h"
#include "luaconfig.h"
#include "lua-options.h"
#include "afinet.h"
#include "afinet-source.h"
#include "afinet-dest.h"

static void afsocket_stream_param_set_keepalive(lua_State* state, void* data)
{
   LogDriver* driver = (LogDriver*) data;
   int keepalive = lua_toboolean(state, -1);
   afsocket_sd_set_keep_alive(driver, keepalive);
}

static void afsocket_stream_param_set_max_conns(lua_State* state, void* data)
{
   LogDriver* driver = (LogDriver*) data;
   int max_conns = lua_tointeger(state, -1);
   afsocket_sd_set_max_connections(driver, max_conns);
}

static void afsocket_register_source_stream_params(LuaOptionParser* parser, LogDriver* driver)
{
   lua_option_parser_add_func(parser, "keep_alive", driver, afsocket_stream_param_set_keepalive);
   lua_option_parser_add_func(parser, "max_connections", driver, afsocket_stream_param_set_max_conns);
}

static void afsocket_register_socket_params(LuaOptionParser* parser, SocketOptions* options)
{
   lua_option_parser_add(parser, "so_sndbuf", LUA_PARSE_TYPE_INT, &options->so_sndbuf);
   lua_option_parser_add(parser, "so_rcvbuf", LUA_PARSE_TYPE_INT, &options->so_rcvbuf);
   lua_option_parser_add(parser, "so_broadcast", LUA_PARSE_TYPE_BOOL, &options->so_broadcast);
   lua_option_parser_add(parser, "so_keepalive", LUA_PARSE_TYPE_BOOL, &options->so_keepalive);
}

static void afsocket_register_unix_source_options(LuaOptionParser* parser, LogDriver* driver)
{
  //TODO: file perm options
  afsocket_register_socket_params(parser, &((AFUnixSourceDriver *) driver)->sock_options);
  lua_option_parser_register_reader_options(parser, &((AFSocketSourceDriver *) driver)->reader_options);
  afsocket_register_source_stream_params(parser, driver);
  lua_option_parser_add(parser, "optional", LUA_PARSE_TYPE_BOOL, &driver->optional);
}

int afsocket_parse_unix_source_options(lua_State* state, LogDriver* d)
{
   LuaOptionParser* parser = lua_option_parser_new();
   afsocket_register_unix_source_options(parser, d);
   lua_option_parser_parse(parser, state);
   lua_option_parser_destroy(parser);
   return 0;
}

static int afsocket_unix_stream_source(lua_State* state)
{
   const char* name = lua_tostring(state, 1);
   LogDriver* d = afunix_sd_new(SOCK_STREAM, name);
   afsocket_parse_unix_source_options(state, d);
   lua_create_userdata_from_pointer(state, d, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

static int afsocket_unix_destination(lua_State* state, int destination_type)
{
   const char* name = lua_tostring(state, 1);
   LogDriver* d = afunix_dd_new(destination_type, name);
   lua_create_userdata_from_pointer(state, d, LUA_DESTINATION_DRIVER_TYPE);
   return 1;
}

static int afsocket_unix_stream_destination(lua_State* state)
{
   return afsocket_unix_destination(state, SOCK_STREAM);
}

static int afsocket_unix_dgram_destination(lua_State* state)
{
   return afsocket_unix_destination(state, SOCK_DGRAM);
}

void afsocket_register_inet_socket_options(LuaOptionParser* parser, SocketOptions* options)
{
   afsocket_register_socket_params(parser, options);
   lua_option_parser_add(parser, "ip_ttl", LUA_PARSE_TYPE_INT, &((InetSocketOptions *) options)->ip_ttl );
   lua_option_parser_add(parser, "ip_tos", LUA_PARSE_TYPE_INT, &((InetSocketOptions *) options)->ip_tos );
   lua_option_parser_add(parser, "tcp_keepalive_time", LUA_PARSE_TYPE_INT, &((InetSocketOptions *) options)->tcp_keepalive_time );
   lua_option_parser_add(parser, "tcp_keepalive_interval", LUA_PARSE_TYPE_INT, &((InetSocketOptions *) options)->tcp_keepalive_intvl );
   lua_option_parser_add(parser, "tcp_keepalive_probes", LUA_PARSE_TYPE_INT, &((InetSocketOptions *) options)->tcp_keepalive_probes );

}

void afsocket_inet_source_option_set_localip(lua_State* state, void* data)
{
   LogDriver* d = (LogDriver*) data;
   char* localip = lua_tostring(state, -1);
   afinet_sd_set_localip(d, localip);
}

void afsocket_inet_source_option_set_localport(lua_State* state, void* data)
{
   LogDriver* d = (LogDriver*) data;
   char* localport = lua_tostring(state, -1);
   afinet_sd_set_localport(d, localport);
}


void afsocket_register_inet_source_options(LuaOptionParser* parser, LogDriver* d)
{
   lua_option_parser_add_func(parser, "localip", d, afsocket_inet_source_option_set_localip);
   lua_option_parser_add_func(parser, "ip", d, afsocket_inet_source_option_set_localip);
   lua_option_parser_add_func(parser, "localport", d, afsocket_inet_source_option_set_localport);
   lua_option_parser_add_func(parser, "port", d, afsocket_inet_source_option_set_localport);
   lua_option_parser_register_reader_options(parser, &((AFSocketSourceDriver *) d)->reader_options);
   afsocket_register_inet_socket_options(parser, &((AFInetSourceDriver *) d)->sock_options.super);
}

int afsocket_parse_udp_source_options(lua_State* state, LogDriver* d)
{
   LuaOptionParser* parser = lua_option_parser_new();
   afsocket_register_inet_source_options(parser, d);
   lua_option_parser_parse(parser, state);
   lua_option_parser_destroy(parser);
   return 0;
}

static int afsocket_udp_source(lua_State* state)
{
   LogDriver* d = afinet_sd_new(AF_INET, SOCK_DGRAM);
   afsocket_parse_udp_source_options(state, d);
   lua_create_userdata_from_pointer(state, d, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

int afsocket_parse_tcp_source_options(lua_State* state, LogDriver* d)
{
   LuaOptionParser* parser = lua_option_parser_new();
   afsocket_register_inet_source_options(parser, d);
   //TODO: TLS
   afsocket_register_source_stream_params(parser, d);
   lua_option_parser_parse(parser, state);
   lua_option_parser_destroy(parser);
   return 0;
}

static int afsocket_tcp_source(lua_State* state)
{
   LogDriver* d = afinet_sd_new(AF_INET, SOCK_STREAM);
   afsocket_parse_tcp_source_options(state, d);
   lua_create_userdata_from_pointer(state, d, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

void afsocket_udp_destination_options_set_spoof_source(lua_State* state, void* data)
{
   LogDriver* d = (LogDriver*) data;
   int spoof_source = lua_toboolean(state, -1);
   afinet_dd_set_spoof_source(d, spoof_source);
}

void afsocket_inet_dest_options_set_localip(lua_State* state, void* data)
{
   LogDriver* d = (LogDriver*) data;
   char* localip = lua_tostring(state, -1);
   afinet_dd_set_localip(d, localip);
}

void afsocket_inet_dest_options_set_localport(lua_State* state, void* data)
{
   LogDriver* d = (LogDriver*) data;
   char* localport = lua_tostring(state, -1);
   afinet_dd_set_localport(d, localport);
}

void afsocket_inet_dest_options_set_destport(lua_State* state, void* data)
{
   LogDriver* d = (LogDriver*) data;
   char* destport = lua_tostring(state, -1);
   afinet_dd_set_destport(d, destport);
}

void afsocket_socket_dest_option_set_keepalive(lua_State* state, void* data)
{
   LogDriver* d = (LogDriver*) data;
   int keepalive = lua_toboolean(state, -1);
   afsocket_dd_set_keep_alive(d, keepalive);
}

void afsocket_register_afsocket_dest_option(LuaOptionParser* parser, LogDriver* d)
{
   lua_option_parser_add_func(parser, "keep_alive", d, afsocket_socket_dest_option_set_keepalive);
}

void afsocket_register_inet_dest_options(LuaOptionParser* parser, LogDriver* d)
{
   lua_option_parser_add_func(parser, "localip", d, afsocket_inet_dest_options_set_localip);
   lua_option_parser_add_func(parser, "localport", d, afsocket_inet_dest_options_set_localport);
   lua_option_parser_add_func(parser, "port", d, afsocket_inet_dest_options_set_destport);
   lua_option_parser_add_func(parser, "destport", d, afsocket_inet_dest_options_set_destport);
   afsocket_register_inet_socket_options(parser, &((AFInetDestDriver *) d)->sock_options.super);
   lua_option_parser_register_writer_options(parser, &((AFSocketDestDriver *) d)->writer_options);
   lua_option_parser_register_destination_options(parser, d);
   afsocket_register_afsocket_dest_option(parser, d);
}

int afsocket_parse_udp_destination_options(lua_State* state, LogDriver* d)
{
   LuaOptionParser* parser = lua_option_parser_new();
   afsocket_register_inet_dest_options(parser, d);
   lua_option_parser_add_func(parser, "spoof_source", d, afsocket_udp_destination_options_set_spoof_source);
   lua_option_parser_parse(parser, state);
   lua_option_parser_destroy(parser);
   return 0;
}

static int afsocket_udp_destination(lua_State* state)
{
   const char* name = lua_tostring(state, 1);
   LogDriver* d = afinet_dd_new(AF_INET, SOCK_DGRAM, name);
   afsocket_parse_udp_destination_options(state, d);
   lua_create_userdata_from_pointer(state, d, LUA_DESTINATION_DRIVER_TYPE);
   return 1;
}

void afsocket_register_lua_config(GlobalConfig* cfg)
{
   lua_State* state = cfg->lua_cfg_state;
   lua_register(state, "UnixStreamSource", afsocket_unix_stream_source); 
   lua_register(state, "UnixStreamDestination", afsocket_unix_stream_destination); 
   //TODO: Unix source and dest should have parameter parsing enabled
   lua_register(state, "TcpSource", afsocket_tcp_source); 
   lua_register(state, "UdpSource", afsocket_udp_source); 
   lua_register(state, "UdpDestination", afsocket_udp_destination); 
}
