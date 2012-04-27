/*
 * Copyright (c) 2010-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2010-2012 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <time.h>

#include "afmongodb.h"
#include "afmongodb-parser.h"
#include "plugin.h"
#include "messages.h"
#include "misc.h"
#include "stats.h"
#include "nvtable.h"
#include "logqueue.h"

#include "mongo.h"

typedef struct
{
  gchar *name;
  LogTemplate *value;
} MongoDBField;

typedef struct
{
  LogThreadedDestDriver super;

  /* Shared between main/writer; only read by the writer, never
     written */
  gchar *db;
  gchar *coll;

  GList *servers;
  gchar *host;
  gint port;

  gboolean safe_mode;

  gchar *user;
  gchar *password;

  time_t last_msg_stamp;

  ValuePairs *vp;

  /* Writer-only stuff */
  mongo_sync_connection *conn;

  gchar *ns;

  GString *current_value;
  bson *bson_sel, *bson_upd, *bson_set;
} MongoDBDestDriver;

/*
 * Configuration
 */

void
afmongodb_dd_set_user(LogDriver *d, const gchar *user)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->user);
  self->user = g_strdup(user);
}

void
afmongodb_dd_set_password(LogDriver *d, const gchar *password)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->password);
  self->password = g_strdup(password);
}

void
afmongodb_dd_set_host(LogDriver *d, const gchar *host)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->host);
  self->host = g_strdup(host);
}

void
afmongodb_dd_set_port(LogDriver *d, gint port)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  self->port = port;
}

void
afmongodb_dd_set_servers(LogDriver *d, GList *servers)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  string_list_free(self->servers);
  self->servers = servers;
}

void
afmongodb_dd_set_database(LogDriver *d, const gchar *database)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->db);
  self->db = g_strdup(database);
}

void
afmongodb_dd_set_collection(LogDriver *d, const gchar *collection)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->coll);
  self->coll = g_strdup(collection);
}

void
afmongodb_dd_set_value_pairs(LogDriver *d, ValuePairs *vp)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  if (self->vp)
    value_pairs_free (self->vp);
  self->vp = vp;
}

void
afmongodb_dd_set_safe_mode(LogDriver *d, gboolean state)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  self->safe_mode = state;
}

/*
 * Utilities
 */

static gchar *
afmongodb_dd_format_stats_instance(MongoDBDestDriver *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
	     "mongodb,%s,%u,%s,%s", self->host, self->port, self->db, self->coll);
  return persist_name;
}

static gchar *
afmongodb_dd_format_persist_name(MongoDBDestDriver *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
	     "afmongodb(%s,%u,%s,%s)", self->host, self->port, self->db, self->coll);
  return persist_name;
}

static void
afmongodb_dd_disconnect(LogThreadedDestDriver *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;

  mongo_sync_disconnect(self->conn);
  self->conn = NULL;
}

static gboolean
afmongodb_dd_connect(MongoDBDestDriver *self, gboolean reconnect)
{
  GList *l;

  if (reconnect && self->conn)
    return TRUE;

  self->conn = mongo_sync_connect(self->host, self->port, FALSE);
  if (!self->conn)
    {
      msg_error ("Error connecting to MongoDB", NULL);
      return FALSE;
    }

  mongo_sync_conn_set_safe_mode(self->conn, self->safe_mode);

  l = self->servers;
  while ((l = g_list_next(l)) != NULL)
    {
      gchar *host = NULL;
      gint port = 27017;

      if (!mongo_util_parse_addr(l->data, &host, &port))
	{
	  msg_warning("Cannot parse MongoDB server address, ignoring",
		      evt_tag_str("address", l->data),
		      NULL);
	  continue;
	}
      mongo_sync_conn_seed_add (self->conn, host, port);
      msg_verbose("Added MongoDB server seed",
		  evt_tag_str("host", host),
		  evt_tag_int("port", port),
		  NULL);
      g_free(host);
    }

  /*
  if (self->user || self->password)
    {
      if (!self->user || !self->password)
	{
	  msg_error("Neither the username, nor the password can be empty", NULL);
	  return FALSE;
	}

      if (mongo_cmd_authenticate(&self->mongo_conn, self->db, self->user, self->password) != 1)
	{
	  msg_error("MongoDB authentication failed", NULL);
	  return FALSE;
	}
    }
  */

  return TRUE;
}

/*
 * Worker thread
 */
static gboolean
afmongodb_vp_foreach (const gchar *name, const gchar *value,
		      gpointer user_data)
{
  bson *bson_set = (bson *)user_data;

  if (name[0] == '.')
    {
      gchar tx_name[256];

      tx_name[0] = '_';
      strncpy(&tx_name[1], name + 1, sizeof(tx_name) - 1);
      tx_name[sizeof(tx_name) - 1] = 0;
      bson_append_string (bson_set, tx_name, value, -1);
    }
  else
    bson_append_string (bson_set, name, value, -1);

  return FALSE;
}

static gboolean
afmongodb_worker_job_method (LogThreadedDestDriver *s, LogMessage *msg,
                             LogPathOptions *path_options)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;
  guint8 *oid;

  bson_reset (self->bson_sel);
  bson_reset (self->bson_upd);
  bson_reset (self->bson_set);

  oid = mongo_util_oid_new_with_time (self->last_msg_stamp, self->super.seq_num);
  bson_append_oid (self->bson_sel, "_id", oid);
  g_free (oid);
  bson_finish (self->bson_sel);

  value_pairs_foreach (self->vp, afmongodb_vp_foreach,
                       msg, self->super.seq_num, self->bson_set);

  bson_finish (self->bson_set);

  bson_append_document (self->bson_upd, "$set", self->bson_set);
  bson_finish (self->bson_upd);

  return mongo_sync_cmd_update (self->conn, self->ns, MONGO_WIRE_FLAG_UPDATE_UPSERT,
                                self->bson_sel, self->bson_upd);
}

static gboolean
afmongodb_worker_thread_init_method (gpointer arg)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)arg;

  afmongodb_dd_connect (self, FALSE);

  self->ns = g_strconcat (self->db, ".", self->coll, NULL);

  self->current_value = g_string_sized_new(256);

  self->bson_sel = bson_new_sized(64);
  self->bson_upd = bson_new_sized(512);
  self->bson_set = bson_new_sized(512);

  return TRUE;
}

static gboolean
afmongodb_worker_thread_deinit_method (gpointer arg)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)arg;

  afmongodb_dd_disconnect(&self->super);

  g_free (self->ns);
  g_string_free (self->current_value, TRUE);

  bson_free (self->bson_sel);
  bson_free (self->bson_upd);
  bson_free (self->bson_set);

  return TRUE;
}

static gboolean
afmongodb_worker_thread_job_method (LogThreadedDestDriver *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;

  afmongodb_dd_connect (self, TRUE);

  return log_threaded_dest_driver_worker_job (s);
}

/*
 * Main thread
 */

static gboolean
afmongodb_dd_init(LogPipe *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  if (!self->vp)
    {
      self->vp = value_pairs_new();
      value_pairs_add_scope(self->vp, "selected-macros");
      value_pairs_add_scope(self->vp, "nv-pairs");
    }

  if (self->host)
    {
      gchar *srv = g_strdup_printf ("%s:%d", self->host,
                                    (self->port) ? self->port : 27017);
      self->servers = g_list_prepend (self->servers, srv);
      g_free (self->host);
    }

  self->host = NULL;
  self->port = 27017;
  if (!mongo_util_parse_addr(g_list_nth_data(self->servers, 0), &self->host,
			     &self->port))
    {
      msg_error("Cannot parse the primary host",
		evt_tag_str("primary", g_list_nth_data(self->servers, 0)),
		NULL);
      return FALSE;
    }

  msg_verbose("Initializing MongoDB destination",
	      evt_tag_str("host", self->host),
	      evt_tag_int("port", self->port),
	      evt_tag_str("database", self->db),
	      evt_tag_str("collection", self->coll),
	      NULL);

  log_threaded_dest_driver_init_method((LogPipe *)&self->super,
                                       afmongodb_dd_format_persist_name(self),
                                       SCS_MONGODB | SCS_DESTINATION,
                                       afmongodb_dd_format_stats_instance(self));

  if (cfg)
    self->super.time_reopen = cfg->time_reopen;

  return TRUE;
}

static gboolean
afmongodb_dd_deinit(LogPipe *s)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;

  return log_threaded_dest_driver_deinit_method(s, SCS_MONGODB | SCS_DESTINATION,
                                                afmongodb_dd_format_stats_instance(self));
}

static void
afmongodb_dd_free(LogPipe *d)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)d;

  g_free(self->db);
  g_free(self->coll);
  g_free(self->user);
  g_free(self->password);
  g_free(self->host);
  string_list_free(self->servers);
  if (self->vp)
    value_pairs_free(self->vp);

  log_threaded_dest_driver_free(d);
}

static void
afmongodb_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  MongoDBDestDriver *self = (MongoDBDestDriver *)s;

  self->last_msg_stamp = cached_g_current_time_sec ();

  log_threaded_dest_driver_queue_method (s, msg, path_options, user_data);
}

/*
 * Plugin glue.
 */

LogDriver *
afmongodb_dd_new(void)
{
  MongoDBDestDriver *self = g_new0(MongoDBDestDriver, 1);

  mongo_util_oid_init (0);

  log_threaded_dest_driver_init_instance (&self->super);

  self->super.worker_job_method = afmongodb_worker_job_method;
  self->super.worker_job_error = afmongodb_dd_disconnect;
  self->super.worker_thread_init_method = afmongodb_worker_thread_init_method;
  self->super.worker_thread_deinit_method = afmongodb_worker_thread_deinit_method;
  self->super.worker_thread_job_method = afmongodb_worker_thread_job_method;

  self->super.super.super.super.init = afmongodb_dd_init;
  self->super.super.super.super.deinit = afmongodb_dd_deinit;
  self->super.super.super.super.queue = afmongodb_dd_queue;
  self->super.super.super.super.free_fn = afmongodb_dd_free;

  afmongodb_dd_set_servers((LogDriver *)self, g_list_append (NULL, g_strdup ("127.0.0.1:27017")));
  afmongodb_dd_set_database((LogDriver *)self, "syslog");
  afmongodb_dd_set_collection((LogDriver *)self, "messages");
  afmongodb_dd_set_safe_mode((LogDriver *)self, FALSE);

  return (LogDriver *)self;
}

extern CfgParser afmongodb_dd_parser;

static Plugin afmongodb_plugin =
{
  .type = LL_CONTEXT_DESTINATION,
  .name = "mongodb",
  .parser = &afmongodb_parser,
};

gboolean
afmongodb_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, &afmongodb_plugin, 1);
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "afmongodb",
  .version = VERSION,
  .description = "The afmongodb module provides MongoDB destination support for syslog-ng.",
  .core_revision = SOURCE_REVISION,
  .plugins = &afmongodb_plugin,
  .plugins_len = 1,
};
