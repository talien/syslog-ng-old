#include "xmpp.h"
#include "logthrdestdrv.h"
#include "plugin.h"
#include "plugin-types.h"
#include "xmpp-parser.h"
#include "stats.h"
#include "xmpp-lib.h"
#include "misc.h"

typedef struct {
  LogThrDestDriver super;

  LogTemplate *msg_template;
  LogTemplate *to_template;

  gchar *server;
  gint port;

  gchar *user;
  gchar *password;

  gint32 seq_num;

  xmpp_session* session;
} XMPPDestDriver;

void
xmpp_dd_set_server(LogDriver* s, char* server)
{
   XMPPDestDriver* self = (XMPPDestDriver*) s;

   g_free(self->server);
   self->server = g_strdup(server);
};

void
xmpp_dd_set_port(LogDriver* s, int port)
{ 
   XMPPDestDriver* self = (XMPPDestDriver*) s;
   self->port = port;
};

void 
xmpp_dd_set_user(LogDriver* s, char* user)
{
   XMPPDestDriver* self = (XMPPDestDriver*) s;
   g_free(self->user);
   self->user = user;
};

void 
xmpp_dd_set_password(LogDriver* s, char* password)
{
   XMPPDestDriver* self = (XMPPDestDriver*) s;
   g_free(self->password);
   self->password = password;
};

void
xmpp_dd_set_to(LogDriver* s, char* to)
{
   XMPPDestDriver* self = (XMPPDestDriver*) s;
   GlobalConfig* cfg = log_pipe_get_config(s);

   if (!self->to_template)
      self->to_template = log_template_new(cfg, NULL);
   log_template_compile(self->to_template, to, NULL);

};

void
xmpp_dd_set_msg(LogDriver* s, char* msg)
{
   XMPPDestDriver* self = (XMPPDestDriver*) s;
   GlobalConfig* cfg = log_pipe_get_config(s);

   if (!self->msg_template)
      self->msg_template = log_template_new(cfg, NULL);
   log_template_compile(self->msg_template, msg, NULL);

};

static int
xmpp_dd_connect(XMPPDestDriver* self, gboolean reconnect)
{
    return TRUE;
};

static void
xmpp_worker_init(LogThrDestDriver *s)
{
    XMPPDestDriver *self = (XMPPDestDriver*) s;
    self->session = xmpp_session_create();
    xmpp_do_start(self->session, self->server, self->port, self->user, self->password);
    xmpp_dd_connect(self, FALSE);
}

static gchar *
xmpp_dd_format_stats_instance(XMPPDestDriver *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name), "xmpp,%s,%u",
             self->server, self->port);
  return persist_name;
}

static gchar *
xmpp_dd_format_persist_name(XMPPDestDriver *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name), "xmpp(%s,%u)",
             self->server, self->port);
  return persist_name;
}

int
xmpp_dd_init(LogPipe* s)
{
   XMPPDestDriver* self = (XMPPDestDriver*) s;
   if (!log_threaded_dest_driver_init_method(&self->super,xmpp_dd_format_persist_name(self),
                                                  SCS_XMPP, xmpp_dd_format_stats_instance(self)))
            return FALSE;


   if ((!self->msg_template) || (!self->to_template) || (!self->server))
   {
      msg_error("msg(), to(), server() fields are mandatory in XMPP destination!\n", NULL);
   }
   log_threaded_dest_driver_start(&self->super);
   return TRUE;   
};

int
xmpp_dd_deinit(LogPipe* s)
{
   XMPPDestDriver* self = (XMPPDestDriver*) s;
   int res;
   res = log_threaded_dest_driver_deinit_method(&self->super,
              SCS_XMPP,
              xmpp_dd_format_stats_instance(self));
   xmpp_session_destroy(self->session);
   return res;
};

void
xmpp_dd_free(LogPipe* s)
{
   XMPPDestDriver* self = (XMPPDestDriver*) s;
   g_free(self->server);
   log_threaded_dest_driver_free(self);
};

static void
xmpp_worker_disconnect(LogThrDestDriver *s)
{
    XMPPDestDriver *self = (XMPPDestDriver *)s;
    xmpp_disconnect(self->session);
}

static int 
xmpp_worker_publish(XMPPDestDriver *self, LogMessage* msg)
{
   GString* message, *to;
   message = g_string_new("");
   to = g_string_new("");
   log_template_format(self->msg_template, msg, NULL, LTZ_LOCAL,
                             self->seq_num, NULL, message);
   log_template_format(self->to_template, msg, NULL, LTZ_LOCAL,
                             self->seq_num, NULL, to);

   printf("XMPP message: %s\n", message->str);
   xmpp_send_msg(self->session, to->str, message->str);
   g_string_free(message, TRUE);
   return TRUE;
};

static int
xmpp_worker_send(LogThrDestDriver *s)
{
  XMPPDestDriver *self = (XMPPDestDriver *)s;
  gboolean success;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  success = log_queue_pop_head(self->super.queue, &msg, &path_options, FALSE, FALSE);
  if (!success)
    return TRUE;

  msg_set_context(msg);
  success = xmpp_worker_publish (self, msg);
  msg_set_context(NULL);

  if (success)
    {
      stats_counter_inc(self->super.stored_messages);
      step_sequence_number(&self->seq_num);
      log_msg_ack(msg, &path_options);
      log_msg_unref(msg);
    }
  else
    {
      log_queue_push_head(self->super.queue, msg, &path_options);
    }

  return success;

}

LogDriver* xmpp_dd_new()
{
   XMPPDestDriver* self = g_new0(XMPPDestDriver,1);
   log_threaded_dest_driver_init_instance(&self->super);
   self->super.super.super.super.init = xmpp_dd_init;
   self->super.super.super.super.deinit = xmpp_dd_deinit;
   self->super.super.super.super.free_fn = xmpp_dd_free;

   self->super.worker.init = xmpp_worker_init;
   self->super.worker.disconnect = xmpp_worker_disconnect;
   self->super.worker.insert = xmpp_worker_send;

   init_sequence_number(&self->seq_num);

   return (LogDriver *) self;
}

extern CfgParser xmpp_dd_parser;

static Plugin xmpp_plugin =
{
  .type = LL_CONTEXT_DESTINATION,
  .name = "xmpp",
  .parser = &xmpp_parser
};

gboolean
xmpp_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, &xmpp_plugin, 1); 
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "xmpp",
  .version = VERSION,
  .description = "The xmpp module provides xmpp/jabber destination support for syslog-ng.",
  .core_revision = SOURCE_REVISION,
  .plugins = &xmpp_plugin,
  .plugins_len = 1,
};

