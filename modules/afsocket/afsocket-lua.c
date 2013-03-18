#include "afsocket-lua.h"
#include "afunix-source.h"
#include "afunix-dest.h"
#include <lua.h>
#include "driver.h"
#include "luaconfig.h"
#include "lua-options.h"
#include "afinet.h"
#include "afinet-source.h"
#include "afinet-dest.h"

#if BUILD_WITH_SSL
#include "tlscontext.h"
#endif

typedef void (*register_func)(LuaOptionParser* parser, LogDriver* d);

static void afsocket_register_and_parse_options(lua_State* state, LogDriver* d, register_func reg_func)
{
   LuaOptionParser* parser = lua_option_parser_new();
   reg_func(parser, d);
   lua_option_parser_parse(parser, state);
   lua_option_parser_destroy(parser);
}

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
  lua_option_parser_register_file_perm_options(parser, &((AFUnixSourceDriver *) driver)->file_perm_options);
  afsocket_register_socket_params(parser, &((AFUnixSourceDriver *) driver)->sock_options);
  lua_option_parser_register_reader_options(parser, &((AFSocketSourceDriver *) driver)->reader_options);
  afsocket_register_source_stream_params(parser, driver);
  lua_option_parser_add(parser, "optional", LUA_PARSE_TYPE_BOOL, &driver->optional);
}

static int afsocket_unix_source(lua_State* state, int source_type)
{
   const char* name = lua_tostring(state, 1);
   LogDriver* d = afunix_sd_new(source_type, name);
   afsocket_register_and_parse_options(state, d, afsocket_register_unix_source_options);
   lua_create_userdata_from_pointer(state, d, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

static int afsocket_unix_stream_source(lua_State* state)
{
   return afsocket_unix_source(state, SOCK_STREAM);
}

static int afsocket_unix_dgram_source(lua_State* state)
{
   return afsocket_unix_source(state, SOCK_DGRAM);
}

static void afsocket_register_unix_destination_options(LuaOptionParser* parser, LogDriver* driver)
{
   lua_option_parser_register_destination_options(parser, driver);
   lua_option_parser_register_writer_options(parser, &((AFSocketDestDriver *) driver)->writer_options);
   afsocket_register_socket_params(parser, &((AFUnixDestDriver *) driver)->sock_options);
   afsocket_register_afsocket_dest_options(parser, driver);
}

static int afsocket_unix_destination(lua_State* state, int destination_type)
{
   const char* name = lua_tostring(state, 1);
   LogDriver* d = afunix_dd_new(destination_type, name);
   afsocket_register_and_parse_options(state, d, afsocket_register_unix_destination_options);
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
   char* localip = g_strdup(lua_tostring(state, -1));
   afinet_sd_set_localip(d, localip);
}

void afsocket_inet_source_option_set_localport(lua_State* state, void* data)
{
   LogDriver* d = (LogDriver*) data;
   char* localport = g_strdup(lua_tostring(state, -1));
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

static int afsocket_udp_source(lua_State* state)
{
   LogDriver* d = afinet_sd_new(AF_INET, SOCK_DGRAM);
   afsocket_register_and_parse_options(state, d, afsocket_register_inet_source_options);
   lua_create_userdata_from_pointer(state, d, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

#if BUILD_WITH_SSL

static void afsocket_tls_set_peer_verify(lua_State* state, void* data)
{
   TLSContext* context = (TLSContext*) data;
   char* verify_mode = lua_tostring(state, -1);
   context->verify_mode = tls_lookup_verify_mode(verify_mode);
}

static GList* afsocket_parse_array_into_glist(lua_State* state)
{
   GList* list = NULL;
   lua_pushnil(state);
   while(lua_next(state, -2)) {
      if(lua_isstring(state, -1))
      {
          g_list_prepend(list, g_strdup(lua_tostring(state, -1)));
      }
      else
      {
         msg_error("Non-string type passed to string list", NULL);
         //TODO: fatal or non-fatal error?
      }
      lua_pop(state, 1);
   }
   list = g_list_reverse(list);
   return list;
}

static void afsocket_tls_set_trusted_keys(lua_State* state, void* data)
{
   TLSContext* context = (TLSContext*) data;
   tls_session_set_trusted_fingerprints(context, afsocket_parse_array_into_glist(state));
}

static void afsocket_tls_set_trusted_dn(lua_State* state, void* data)
{
   TLSContext* context = (TLSContext*) data;
   tls_session_set_trusted_dn(context, afsocket_parse_array_into_glist(state));
}

static void afsocket_register_tls_options(LuaOptionParser* parser, TLSContext* context)
{
   lua_option_parser_add_func(parser, "peer_verify", context, afsocket_tls_set_peer_verify);
   lua_option_parser_add(parser, "key_file", LUA_PARSE_TYPE_STR, &context->key_file);
   lua_option_parser_add(parser, "cert_file", LUA_PARSE_TYPE_STR, &context->cert_file);
   lua_option_parser_add(parser, "ca_dir", LUA_PARSE_TYPE_STR, &context->ca_dir);
   lua_option_parser_add(parser, "crl_dir", LUA_PARSE_TYPE_STR, &context->crl_dir);
   lua_option_parser_add_func(parser, "trusted_keys", context, afsocket_tls_set_trusted_keys);
   lua_option_parser_add_func(parser, "trusted_dn", context, afsocket_tls_set_trusted_dn);
   lua_option_parser_add(parser, "cipher_suite", LUA_PARSE_TYPE_STR, &context->cipher_suite);
}

typedef void (*tls_set_context_func)(LogDriver* d, TLSContext* context);

static void afsocket_tls_options(lua_State* state, LogDriver* d, int tls_type, tls_set_context_func func)
{
   TLSContext* context = tls_context_new(tls_type);
   LuaOptionParser* parser = lua_option_parser_new();
   afsocket_register_tls_options(parser, context);
   lua_option_parser_parse(parser, state);
   lua_option_parser_destroy(parser);
   func(d, context);
}

static void afsocket_source_tls_options(lua_State* state, LogDriver* d)
{
   afsocket_tls_options(state, d, TM_SERVER, afsocket_sd_set_tls_context);
}

static void afsocket_destination_tls_options(lua_State* state, LogDriver* d)
{
   afsocket_tls_options(state, d, TM_CLIENT, afsocket_dd_set_tls_context);
}
#endif

static void afsocket_register_tls_source_options(LuaOptionParser* parser, LogDriver* d)
{
   #if BUILD_WITH_SSL
   lua_option_parser_add_func(parser, "tls", d, afsocket_source_tls_options);
   #endif
}

static void afsocket_register_tls_destination_options(LuaOptionParser* parser, LogDriver* d)
{
   #if BUILD_WITH_SSL
   lua_option_parser_add_func(parser, "tls", d, afsocket_destination_tls_options);
   #endif
}

static void afsocket_register_tcp_source_options(LuaOptionParser* parser, LogDriver* d)
{
   afsocket_register_inet_source_options(parser, d);
   afsocket_register_tls_source_options(parser, d);
   afsocket_register_source_stream_params(parser, d);
}

static int afsocket_tcp_source(lua_State* state)
{
   LogDriver* d = afinet_sd_new(AF_INET, SOCK_STREAM);
   afsocket_register_and_parse_options(state, d, afsocket_register_tcp_source_options);
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
   char* localip = g_strdup(lua_tostring(state, -1));
   afinet_dd_set_localip(d, localip);
}

void afsocket_inet_dest_options_set_localport(lua_State* state, void* data)
{
   LogDriver* d = (LogDriver*) data;
   char* localport = g_strdup(lua_tostring(state, -1));
   afinet_dd_set_localport(d, localport);
}

void afsocket_inet_dest_options_set_destport(lua_State* state, void* data)
{
   LogDriver* d = (LogDriver*) data;
   char* destport = g_strdup(lua_tostring(state, -1));
   afinet_dd_set_destport(d, destport);
}

void afsocket_socket_dest_option_set_keepalive(lua_State* state, void* data)
{
   LogDriver* d = (LogDriver*) data;
   int keepalive = lua_toboolean(state, -1);
   afsocket_dd_set_keep_alive(d, keepalive);
}

void afsocket_register_afsocket_dest_options(LuaOptionParser* parser, LogDriver* d)
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
   afsocket_register_afsocket_dest_options(parser, d);
}

void afsocket_register_udp_destination_options(LuaOptionParser* parser, LogDriver* d)
{
   afsocket_register_inet_dest_options(parser, d);
   lua_option_parser_add_func(parser, "spoof_source", d, afsocket_udp_destination_options_set_spoof_source);
}

static int afsocket_udp_destination(lua_State* state)
{
   const char* name = lua_tostring(state, 1);
   LogDriver* d = afinet_dd_new(AF_INET, SOCK_DGRAM, name);
   afsocket_register_and_parse_options(state, d, afsocket_register_udp_destination_options);
   lua_create_userdata_from_pointer(state, d, LUA_DESTINATION_DRIVER_TYPE);
   return 1;
}

void afsocket_register_tcp_destination_options(LuaOptionParser* parser, LogDriver* d)
{
   afsocket_register_inet_dest_options(parser, d);
   afsocket_register_tls_destination_options(parser, d);
}

static int afsocket_tcp_destination(lua_State* state)
{
   const char* name = lua_tostring(state, 1);
   LogDriver* d = afinet_dd_new(AF_INET, SOCK_STREAM, name);
   afsocket_register_and_parse_options(state, d, afsocket_register_tcp_destination_options);
   lua_create_userdata_from_pointer(state, d, LUA_DESTINATION_DRIVER_TYPE);
   return 1;
}

static void afsocket_source_transport_option_set_transport(lua_State* state, void* data)
{
   LogDriver* driver = (LogDriver*) data;
   const char* transport = g_strdup(lua_tostring(state, -1));
   afsocket_sd_set_transport(driver, transport); 
}

static void afsocket_register_source_transport_options(LuaOptionParser* parser, LogDriver* driver)
{
   lua_option_parser_add_func(parser, "transport", driver, afsocket_source_transport_option_set_transport);
   afsocket_register_tls_source_options(parser, driver);
}

static void afsocket_register_syslog_source_options(LuaOptionParser* parser, LogDriver* driver)
{
   afsocket_register_inet_source_options(parser, driver);
   afsocket_register_source_stream_params(parser, driver);
   afsocket_register_source_transport_options(parser, driver); 
}

static int afsocket_syslog_source(lua_State* state)
{
   LogDriver* d = afsyslog_sd_new();
   afsocket_register_and_parse_options(state, d, afsocket_register_syslog_source_options);
   lua_create_userdata_from_pointer(state, d, LUA_SOURCE_DRIVER_TYPE);
   return 1;
}

static void afsocket_dest_transport_option_set_transport(lua_State* state, void* data)
{
   LogDriver* driver = (LogDriver*) data;
   const char* transport = g_strdup(lua_tostring(state, -1));
   afsocket_dd_set_transport(driver, transport);
}

static void afsocket_register_dest_transport_options(LuaOptionParser* parser, LogDriver* driver)
{
   lua_option_parser_add_func(parser, "transport", driver, afsocket_dest_transport_option_set_transport);
   afsocket_register_tls_destination_options(parser, driver);
}

static void afsocket_register_syslog_destination_options(LuaOptionParser* parser, LogDriver* driver)
{
   afsocket_register_inet_dest_options(parser, driver);
   afsocket_register_dest_transport_options(parser, driver);
}

static int afsocket_syslog_destination(lua_State* state)
{
   const char* host = lua_tostring(state, 1);
   LogDriver* d = afsyslog_dd_new(host);
   afsocket_register_and_parse_options(state, d, afsocket_register_syslog_destination_options);
   lua_create_userdata_from_pointer(state, d, LUA_DESTINATION_DRIVER_TYPE);
   return 1;
}

void afsocket_register_lua_config(GlobalConfig* cfg)
{
   lua_State* state = cfg->lua_cfg_state;
   lua_register(state, "UnixStreamSource", afsocket_unix_stream_source); 
   lua_register(state, "UnixStreamDestination", afsocket_unix_stream_destination); 
   lua_register(state, "UnixDgramSource", afsocket_unix_dgram_source); 
   lua_register(state, "UnixDgramDestination", afsocket_unix_dgram_destination); 
   lua_register(state, "TcpSource", afsocket_tcp_source); 
   lua_register(state, "UdpSource", afsocket_udp_source); 
   lua_register(state, "UdpDestination", afsocket_udp_destination); 
   lua_register(state, "TcpDestination", afsocket_tcp_destination); 
   lua_register(state, "SyslogSource", afsocket_syslog_source);
   lua_register(state, "SyslogDestination", afsocket_syslog_destination);
}
