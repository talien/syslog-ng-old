/*
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
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

#include "redisdd.h"
#include "redisdd-parser.h"
#include "plugin.h"
#include "messages.h"
#include "misc.h"
#include "stats.h"
#include "nvtable.h"
#include "logqueue.h"

#include <hiredis/hiredis.h>

typedef struct
{
  gchar *name;
  LogTemplate *value;
} MongoDBField;

typedef struct
{
  LogDestDriver super;

  /* Shared between main/writer; only read by the writer, never
     written */
  gchar *host;
  gint port;

  gchar *command;
  gchar *prefix;

  time_t time_reopen;

  StatsCounterItem *dropped_messages;
  StatsCounterItem *stored_messages;

  time_t last_msg_stamp;

  ValuePairs *vp;

  /* Thread related stuff; shared */
  GThread *writer_thread;
  GMutex *queue_mutex;
  GMutex *suspend_mutex;
  GCond *writer_thread_wakeup_cond;

  gboolean writer_thread_terminate;
  gboolean writer_thread_suspended;
  GTimeVal writer_thread_suspend_target;

  LogQueue *queue;

  /* Writer-only stuff */
  redisContext *redisctx;
  gint32 seq_num;
} RedisDD;

/*
 * Configuration
 */

void
redis_dd_set_host(LogDriver *d, const gchar *host)
{
  RedisDD *self = (RedisDD *)d;

  g_free(self->host);
  self->host = g_strdup (host);
}

void
redis_dd_set_port(LogDriver *d, gint port)
{
  RedisDD *self = (RedisDD *)d;

  self->port = (int)port;
}

void
redis_dd_set_command(LogDriver *d, const gchar *cmd)
{
  RedisDD *self = (RedisDD *)d;

  g_free(self->command);
  self->command = g_strdup(cmd);
}

void
redis_dd_set_prefix(LogDriver *d, const gchar *p)
{
  RedisDD *self = (RedisDD *)d;

  g_free(self->prefix);
  self->prefix = g_strdup(p);
}

void
redis_dd_set_value_pairs(LogDriver *d, ValuePairs *vp)
{
  RedisDD *self = (RedisDD *)d;

  if (self->vp)
    value_pairs_free (self->vp);
  self->vp = vp;
}

/*
 * Utilities
 */

static gchar *
redis_dd_format_stats_instance(RedisDD *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
             "redis,%s,%u", self->host, self->port);
  return persist_name;
}

static gchar *
redis_dd_format_persist_name(RedisDD *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
             "redis(%s,%u)", self->host, self->port);
  return persist_name;
}

static void
redis_dd_suspend(RedisDD *self)
{
  self->writer_thread_suspended = TRUE;
  g_get_current_time(&self->writer_thread_suspend_target);
  g_time_val_add(&self->writer_thread_suspend_target,
                 self->time_reopen * 1000000);
}

static void
redis_dd_disconnect(RedisDD *self)
{
  redisFree(self->redisctx);
  self->redisctx = NULL;
}

static gboolean
redis_dd_connect(RedisDD *self, gboolean reconnect)
{
  if (reconnect && self->redisctx)
    return TRUE;

  self->redisctx = redisConnect(self->host, self->port);

  if (self->redisctx->err)
    {
      msg_error ("Error connecting to redis",
                 evt_tag_str("error", self->redisctx->errstr),
                 NULL);
      redis_dd_disconnect(self);
      return FALSE;
    }

  return TRUE;
}

/*
 * Worker thread
 */
static gboolean
redis_dd_vp_foreach (const gchar *name, const gchar *value,
                     gpointer user_data)
{
  RedisDD *self = ((gpointer *)user_data)[0];
  gboolean *success = ((gpointer *)user_data)[1];
  redisReply *reply;

  reply = redisCommand(self->redisctx, "%s %s%s %s",
                       self->command, self->prefix,
                       name, value);
  if (!reply)
    {
      msg_error("Error communicating with redis",
                evt_tag_str("error", self->redisctx->errstr),
                evt_tag_int("time_reopen", self->time_reopen),
                NULL);
      *success = FALSE;
      return TRUE;
    }

  freeReplyObject(reply);

  return FALSE;
}

static gboolean
redis_dd_worker_insert (RedisDD *self)
{
  gboolean success = TRUE;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gpointer params[] = { self, &success };

  redis_dd_connect(self, TRUE);

  g_mutex_lock(self->queue_mutex);
  log_queue_reset_parallel_push(self->queue);
  success = log_queue_pop_head(self->queue, &msg, &path_options, FALSE, FALSE);
  g_mutex_unlock(self->queue_mutex);
  if (!success)
    return TRUE;

  msg_set_context(msg);

  value_pairs_foreach (self->vp, redis_dd_vp_foreach,
                       msg, self->seq_num, params);

  msg_set_context(NULL);

  if (success)
    {
      stats_counter_inc(self->stored_messages);
      step_sequence_number(&self->seq_num);
      log_msg_ack(msg, &path_options);
      log_msg_unref(msg);
    }
  else
    {
      g_mutex_lock(self->queue_mutex);
      log_queue_push_head(self->queue, msg, &path_options);
      g_mutex_unlock(self->queue_mutex);
    }

  return success;
}

static gpointer
redis_dd_worker_thread (gpointer arg)
{
  RedisDD *self = (RedisDD *)arg;

  msg_debug ("Worker thread started",
             evt_tag_str("driver", self->super.super.id),
             NULL);

  redis_dd_connect(self, FALSE);

  while (!self->writer_thread_terminate)
    {
      g_mutex_lock(self->suspend_mutex);
      if (self->writer_thread_suspended)
        {
          g_cond_timed_wait(self->writer_thread_wakeup_cond,
                            self->suspend_mutex,
                            &self->writer_thread_suspend_target);
          self->writer_thread_suspended = FALSE;
          g_mutex_lock(self->queue_mutex);
          log_queue_reset_parallel_push(self->queue);
          g_mutex_unlock(self->queue_mutex);
          g_mutex_unlock(self->suspend_mutex);
        }
      else
        {
          g_mutex_unlock(self->suspend_mutex);

          g_mutex_lock(self->queue_mutex);
          if (log_queue_get_length(self->queue) == 0)
            {
              g_cond_wait(self->writer_thread_wakeup_cond, self->queue_mutex);
              log_queue_reset_parallel_push(self->queue);
            }
          g_mutex_unlock(self->queue_mutex);
        }

      if (self->writer_thread_terminate)
        break;

      if (!redis_dd_worker_insert (self))
        {
          redis_dd_disconnect(self);
          redis_dd_suspend(self);
        }
    }

  redis_dd_disconnect(self);

  msg_debug ("Worker thread finished",
             evt_tag_str("driver", self->super.super.id),
             NULL);

  return NULL;
}

/*
 * Main thread
 */

static void
redis_dd_start_thread (RedisDD *self)
{
  self->writer_thread = create_worker_thread(redis_dd_worker_thread, self, TRUE, NULL);
}

static void
redis_dd_stop_thread (RedisDD *self)
{
  self->writer_thread_terminate = TRUE;
  g_mutex_lock(self->queue_mutex);
  g_cond_signal(self->writer_thread_wakeup_cond);
  g_mutex_unlock(self->queue_mutex);
  g_thread_join(self->writer_thread);
}

static gboolean
redis_dd_init(LogPipe *s)
{
  RedisDD *self = (RedisDD *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  if (cfg)
    self->time_reopen = cfg->time_reopen;

  if (!self->vp)
    {
      self->vp = value_pairs_new();
      value_pairs_add_scope(self->vp, "selected-macros");
      value_pairs_add_scope(self->vp, "nv-pairs");
    }

  msg_verbose("Initializing Redis destination",
              evt_tag_str("host", self->host),
              evt_tag_int("port", self->port),
              NULL);

  self->queue = log_dest_driver_acquire_queue(&self->super, redis_dd_format_persist_name(self));

  stats_lock();
  stats_register_counter(0, SCS_REDIS | SCS_DESTINATION, self->super.super.id,
                         redis_dd_format_stats_instance(self),
                         SC_TYPE_STORED, &self->stored_messages);
  stats_register_counter(0, SCS_REDIS | SCS_DESTINATION, self->super.super.id,
                         redis_dd_format_stats_instance(self),
                         SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unlock();

  log_queue_set_counters(self->queue, self->stored_messages, self->dropped_messages);
  redis_dd_start_thread(self);

  return TRUE;
}

static gboolean
redis_dd_deinit(LogPipe *s)
{
  RedisDD *self = (RedisDD *)s;

  redis_dd_stop_thread(self);

  log_queue_set_counters(self->queue, NULL, NULL);
  stats_lock();
  stats_unregister_counter(SCS_REDIS | SCS_DESTINATION, self->super.super.id,
                           redis_dd_format_stats_instance(self),
                           SC_TYPE_STORED, &self->stored_messages);
  stats_unregister_counter(SCS_REDIS | SCS_DESTINATION, self->super.super.id,
                           redis_dd_format_stats_instance(self),
                           SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unlock();
  if (!log_dest_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

static void
redis_dd_free(LogPipe *d)
{
  RedisDD *self = (RedisDD *)d;

  g_mutex_free(self->suspend_mutex);
  g_mutex_free(self->queue_mutex);
  g_cond_free(self->writer_thread_wakeup_cond);

  if (self->queue)
    log_queue_unref(self->queue);

  g_free(self->host);
  if (self->vp)
    value_pairs_free(self->vp);
  log_dest_driver_free(d);
}

static void
redis_dd_queue_notify(gpointer user_data)
{
  RedisDD *self = (RedisDD *)user_data;

  g_cond_signal(self->writer_thread_wakeup_cond);
}

static void
redis_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  RedisDD *self = (RedisDD *)s;
  LogPathOptions local_options;

  if (!path_options->flow_control_requested)
    path_options = log_msg_break_ack(msg, path_options, &local_options);

  self->last_msg_stamp = cached_g_current_time_sec ();

  g_mutex_lock(self->suspend_mutex);
  g_mutex_lock(self->queue_mutex);
  if (!self->writer_thread_suspended)
    log_queue_set_parallel_push(self->queue, 1, redis_dd_queue_notify,
                                self, NULL);
  g_mutex_unlock(self->queue_mutex);
  g_mutex_unlock(self->suspend_mutex);
  log_queue_push_tail(self->queue, msg, path_options);
}

/*
 * Plugin glue.
 */

LogDriver *
redis_dd_new(void)
{
  RedisDD *self = g_new0(RedisDD, 1);

  log_dest_driver_init_instance(&self->super);
  self->super.super.super.init = redis_dd_init;
  self->super.super.super.deinit = redis_dd_deinit;
  self->super.super.super.queue = redis_dd_queue;
  self->super.super.super.free_fn = redis_dd_free;

  redis_dd_set_host((LogDriver *)self, "127.0.0.1");
  redis_dd_set_port((LogDriver *)self, 6379);
  redis_dd_set_command((LogDriver *)self, "LPUSH");
  redis_dd_set_prefix((LogDriver *)self, "");

  init_sequence_number(&self->seq_num);

  self->writer_thread_wakeup_cond = g_cond_new();
  self->suspend_mutex = g_mutex_new();
  self->queue_mutex = g_mutex_new();

  return (LogDriver *)self;
}

extern CfgParser redis_dd_parser;

static Plugin redis_dd_plugin =
{
  .type = LL_CONTEXT_DESTINATION,
  .name = "redis",
  .parser = &redis_dd_parser,
};

gboolean
redisdd_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, &redis_dd_plugin, 1);
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "redisdd",
  .version = VERSION,
  .description = "The redisdd module provides Redis destination support for syslog-ng.",
  .core_revision = SOURCE_REVISION,
  .plugins = &redis_dd_plugin,
  .plugins_len = 1,
};
