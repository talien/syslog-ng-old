/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
  
#include "driver.h"
#include "logqueue-fifo.h"
#include "afinter.h"
#include "cfg-tree.h"
#include "misc.h"

/* LogDriverPlugin */

void
log_driver_plugin_free_method(LogDriverPlugin *self)
{
  g_free(self);
}

void
log_driver_plugin_init_instance(LogDriverPlugin *self)
{
  self->free_fn = log_driver_plugin_free_method;
}

/* LogDriver */

void
log_driver_add_plugin(LogDriver *self, LogDriverPlugin *plugin)
{
  self->plugins = g_list_append(self->plugins, plugin);
}

gboolean
log_driver_init_method(LogPipe *s)
{
  LogDriver *self = (LogDriver *) s;
  gboolean success = TRUE;
  GList *l;

  for (l = self->plugins; l; l = l->next)
    {
      if (!log_driver_plugin_attach((LogDriverPlugin *) l->data, self))
        success = FALSE;
    }
  return success;
}

gboolean
log_driver_deinit_method(LogPipe *s)
{
  LogDriver *self = (LogDriver *) s;
  gboolean success = TRUE;
  GList *l;

  for (l = self->plugins; l; l = l->next)
    {
      log_driver_plugin_detach((LogDriverPlugin *) l->data, self);
    }
  return success;
}

/* NOTE: intentionally static, as only cDriver or LogDestDriver will derive from LogDriver */
static void
log_driver_free(LogPipe *s)
{
  LogDriver *self = (LogDriver *) s;
  GList *l;
  
  for (l = self->plugins; l; l = l->next)
    {
      log_driver_plugin_free((LogDriverPlugin *) l->data);
    }
  if (self->group)
    g_free(self->group);
  if (self->id)
    g_free(self->id);
  log_pipe_free_method(s);
}

/* NOTE: intentionally static, as only LogSrcDriver or LogDestDriver will derive from LogDriver */
static void
log_driver_init_instance(LogDriver *self)
{
  log_pipe_init_instance(&self->super);
  self->super.free_fn = log_driver_free;
  self->super.init = log_driver_init_method;
  self->super.deinit = log_driver_deinit_method;
}

/* LogSrcDriver */

gboolean
log_src_driver_init_method(LogPipe *s)
{
  LogSrcDriver *self = (LogSrcDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_driver_init_method(s))
    return FALSE;

  if (!self->super.group)
    {
      self->super.group = cfg_tree_get_rule_name(&cfg->tree, ENC_SOURCE, s->expr_node);
      self->group_len = strlen(self->super.group);
      self->super.id = cfg_tree_get_child_id(&cfg->tree, ENC_SOURCE, s->expr_node);
    }

  stats_lock();
  stats_register_counter(0, SCS_SOURCE | SCS_GROUP, self->super.group, NULL, SC_TYPE_PROCESSED, &self->super.processed_group_messages);
  stats_register_counter(0, SCS_CENTER, NULL, "received", SC_TYPE_PROCESSED, &self->received_global_messages);
  stats_unlock();

  return TRUE;
}

gboolean
log_src_driver_deinit_method(LogPipe *s)
{
  LogSrcDriver *self = (LogSrcDriver *) s;

  if (!log_driver_deinit_method(s))
    return FALSE;

  stats_lock();
  stats_unregister_counter(SCS_SOURCE | SCS_GROUP, self->super.group, NULL, SC_TYPE_PROCESSED, &self->super.processed_group_messages);
  stats_unregister_counter(SCS_CENTER, NULL, "received", SC_TYPE_PROCESSED, &self->received_global_messages);
  stats_unlock();
  return TRUE;
}

void
log_src_driver_queue_method(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  LogSrcDriver *self = (LogSrcDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  /* $SOURCE */

  if (msg->flags & LF_LOCAL)
    afinter_postpone_mark(cfg->mark_freq);

  log_msg_set_value(msg, LM_V_SOURCE, self->super.group, self->group_len);
  stats_counter_inc(self->super.processed_group_messages);
  stats_counter_inc(self->received_global_messages);
  log_pipe_forward_msg(s, msg, path_options);
}

void
log_src_driver_init_instance(LogSrcDriver *self)
{
  log_driver_init_instance(&self->super);
  self->super.super.init = log_src_driver_init_method;
  self->super.super.deinit = log_src_driver_deinit_method;
  self->super.super.queue = log_src_driver_queue_method;
  self->super.super.flags |= PIF_SOURCE;
}

void
log_src_driver_free(LogPipe *s)
{
  log_driver_free(s);
}

/* LogDestDriver */

/* returns a reference */
static LogQueue *
log_dest_driver_acquire_queue_method(LogDestDriver *self, gchar *persist_name, gpointer user_data)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super);
  LogQueue *queue = NULL;

  g_assert(user_data == NULL);

  if (persist_name)
    queue = cfg_persist_config_fetch(cfg, persist_name);

  if (!queue)
    {
      queue = log_queue_fifo_new(self->log_fifo_size < 0 ? cfg->log_fifo_size : self->log_fifo_size, persist_name);
      log_queue_set_throttle(queue, self->throttle);
    }
  return queue;
}

/* consumes the reference in @q */
static void
log_dest_driver_release_queue_method(LogDestDriver *self, LogQueue *q, gpointer user_data)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super);

  /* we only save the LogQueue instance if it contains data */
  if (q->persist_name && log_queue_keep_on_reload(q) > 0)
    cfg_persist_config_add(cfg, q->persist_name, q, (GDestroyNotify) log_queue_unref, FALSE);
  else
    log_queue_unref(q);
}

void
log_dest_driver_queue_method(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  LogDestDriver *self = (LogDestDriver *) s;

  stats_counter_inc(self->super.processed_group_messages);
  stats_counter_inc(self->queued_global_messages);
  log_pipe_forward_msg(s, msg, path_options);
}

gboolean
log_dest_driver_init_method(LogPipe *s)
{
  LogDestDriver *self = (LogDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_driver_init_method(s))
    return FALSE;

  if (!self->super.group)
    {
      self->super.group = cfg_tree_get_rule_name(&cfg->tree, ENC_DESTINATION, s->expr_node);
      self->super.id = cfg_tree_get_child_id(&cfg->tree, ENC_DESTINATION, s->expr_node);
    }

  stats_lock();
  stats_register_counter(0, SCS_DESTINATION | SCS_GROUP, self->super.group, NULL, SC_TYPE_PROCESSED, &self->super.processed_group_messages);
  stats_register_counter(0, SCS_CENTER, NULL, "queued", SC_TYPE_PROCESSED, &self->queued_global_messages);
  stats_unlock();

  return TRUE;
}

gboolean
log_dest_driver_deinit_method(LogPipe *s)
{
  LogDestDriver *self = (LogDestDriver *) s;
  GList *l, *l_next;

  for (l = self->queues; l; l = l_next)
    {
      LogQueue *q = (LogQueue *) l->data;

      /* the GList struct will be freed by log_dest_driver_release_queue */
      l_next = l->next;

      /* we have to pass a reference to log_dest_driver_release_queue(),
       * which automatically frees the ref on the list too */
      log_dest_driver_release_queue(self, log_queue_ref(q));
    }
  g_assert(self->queues == NULL);

  stats_lock();
  stats_unregister_counter(SCS_DESTINATION | SCS_GROUP, self->super.group, NULL, SC_TYPE_PROCESSED, &self->super.processed_group_messages);
  stats_unregister_counter(SCS_CENTER, NULL, "queued", SC_TYPE_PROCESSED, &self->queued_global_messages);
  stats_unlock();

  if (!log_driver_deinit_method(s))
    return FALSE;
  return TRUE;
}

void
log_dest_driver_init_instance(LogDestDriver *self)
{
  log_driver_init_instance(&self->super);
  self->super.super.init = log_dest_driver_init_method;
  self->super.super.deinit = log_dest_driver_deinit_method;
  self->super.super.queue = log_dest_driver_queue_method;
  self->acquire_queue = log_dest_driver_acquire_queue_method;
  self->release_queue = log_dest_driver_release_queue_method;
  self->log_fifo_size = -1;
  self->throttle = 0;
}

void
log_dest_driver_free(LogPipe *s)
{
  LogDestDriver *self = (LogDestDriver *) s;
  GList *l;

  for (l = self->queues; l; l = l->next)
    {
      log_queue_unref((LogQueue *) l->data);
    }
  g_list_free(self->queues);
  log_driver_free(s);
}

/* Threaded Destination Driver */

static void
log_threaded_dest_driver_worker_suspend(LogThreadedDestDriver *self)
{
  self->worker_thread_suspended = TRUE;
  g_get_current_time(&self->worker_thread_suspend_target);
  g_time_val_add(&self->worker_thread_suspend_target,
                 self->time_reopen * 1000000);
}

static void
log_threaded_dest_driver_start_worker(LogThreadedDestDriver *self)
{
  self->worker_thread = create_worker_thread(self->worker_thread_method, self, TRUE, NULL);
}

static void
log_threaded_dest_driver_stop_worker(LogThreadedDestDriver *self)
{
  self->worker_thread_terminate = TRUE;
  g_mutex_lock(self->queue_mutex);
  g_cond_signal(self->worker_thread_wakeup_cond);
  g_mutex_unlock(self->queue_mutex);
  g_thread_join(self->worker_thread);
}

gboolean
log_threaded_dest_driver_init_method(LogPipe *s, gchar *persist_name,
                                     gint stats_source, const gchar *stats_instance)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;

  self->queue = log_dest_driver_acquire_queue(&self->super, persist_name);

  stats_lock();
  stats_register_counter(0, stats_source, self->super.super.id,
                         stats_instance,
                         SC_TYPE_STORED, &self->stored_messages);
  stats_register_counter(0, stats_source, self->super.super.id,
                         stats_instance,
                         SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unlock();

  log_queue_set_counters(self->queue, self->stored_messages, self->dropped_messages);
  log_threaded_dest_driver_start_worker(self);

  return TRUE;
}

gboolean
log_threaded_dest_driver_deinit_method(LogPipe *s, gint stats_source,
                                       const gchar *stats_instance)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;

  log_threaded_dest_driver_stop_worker(self);
  log_queue_set_counters(self->queue, NULL, NULL);

  stats_lock();
  stats_unregister_counter(stats_source, self->super.super.id,
                           stats_instance,
                           SC_TYPE_STORED, &self->stored_messages);
  stats_unregister_counter(stats_source, self->super.super.id,
                           stats_instance,
                           SC_TYPE_DROPPED, &self->dropped_messages);
  stats_unlock();

  return log_dest_driver_deinit_method(s);
}

void
log_threaded_dest_driver_free(LogPipe *s)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;

  g_mutex_free(self->suspend_mutex);
  g_mutex_free(self->queue_mutex);
  g_cond_free(self->worker_thread_wakeup_cond);

  if (self->queue)
    log_queue_unref(self->queue);

  log_dest_driver_free(s);
}

static void
log_threaded_dest_driver_queue_notify(gpointer user_data)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)user_data;

  g_mutex_lock(self->queue_mutex);
  g_cond_signal(self->worker_thread_wakeup_cond);
  log_queue_reset_parallel_push(self->queue);
  g_mutex_unlock(self->queue_mutex);
}

void
log_threaded_dest_driver_queue_method(LogPipe *s, LogMessage *msg,
                                      const LogPathOptions *path_options,
                                      gpointer user_data)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)s;
  LogPathOptions local_options;

  if (!path_options->flow_control_requested)
    path_options = log_msg_break_ack(msg, path_options, &local_options);

  g_mutex_lock(self->suspend_mutex);
  g_mutex_lock(self->queue_mutex);

  log_msg_add_ack(msg, path_options);
  log_queue_push_tail(self->queue, log_msg_ref(msg), path_options);

  if (!self->worker_thread_suspended)
    log_queue_set_parallel_push(self->queue, 1,
                                log_threaded_dest_driver_queue_notify,
                                self, NULL);
  g_mutex_unlock(self->queue_mutex);
  g_mutex_unlock(self->suspend_mutex);

  log_dest_driver_queue_method(s, msg, path_options, user_data);
}

void
log_threaded_dest_driver_init_instance(LogThreadedDestDriver *self)
{
  log_dest_driver_init_instance(&self->super);

  self->worker_thread_wakeup_cond = g_cond_new();
  self->suspend_mutex = g_mutex_new();
  self->queue_mutex = g_mutex_new();

  init_sequence_number(&self->seq_num);

  self->worker_thread_method = log_threaded_dest_driver_worker_thread;
  self->worker_thread_job_method = log_threaded_dest_driver_worker_job;

  self->super.super.super.queue = log_threaded_dest_driver_queue_method;
  self->super.super.super.free_fn = log_threaded_dest_driver_free;
}

static gboolean
log_threaded_dest_driver_queue_pop_head(LogThreadedDestDriver *self,
                                        LogMessage **msg,
                                        LogPathOptions *path_options)
{
  gboolean success;

  g_mutex_lock(self->queue_mutex);
  log_queue_reset_parallel_push(self->queue);
  success = log_queue_pop_head(self->queue, msg, path_options, FALSE, FALSE);
  g_mutex_unlock(self->queue_mutex);

  return success;
}

gboolean
log_threaded_dest_driver_worker_job(LogThreadedDestDriver *self)
{
  gboolean success;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  if (!log_threaded_dest_driver_queue_pop_head(self, &msg, &path_options))
    return TRUE;

  msg_set_context(msg);
  success = self->worker_job_method(self, msg, &path_options);
  msg_set_context(NULL);

  if (!success)
    {
      msg_error("Error during threaded worker execution",
                evt_tag_str("driver", self->super.super.id),
                evt_tag_int("time_reopen", self->time_reopen),
                NULL);

      g_mutex_lock(self->queue_mutex);
      log_queue_push_head(self->queue, msg, &path_options);
      g_mutex_unlock(self->queue_mutex);
    }
  else
    {
      stats_counter_inc(self->stored_messages);
      step_sequence_number(&self->seq_num);
      log_msg_ack(msg, &path_options);
      log_msg_unref(msg);
    }

  return success;
}

gpointer
log_threaded_dest_driver_worker_thread(gpointer arg)
{
  LogThreadedDestDriver *self = (LogThreadedDestDriver *)arg;

  msg_debug("Worker thread started",
            evt_tag_str("driver", self->super.super.id),
            NULL);

  if (self->worker_thread_init_method)
    self->worker_thread_init_method(self);

  while (!self->worker_thread_terminate)
    {
      g_mutex_lock(self->suspend_mutex);
      if (self->worker_thread_suspended)
        {
          g_cond_timed_wait(self->worker_thread_wakeup_cond,
                            self->suspend_mutex,
                            &self->worker_thread_suspend_target);
          self->worker_thread_suspended = FALSE;
          g_mutex_unlock(self->suspend_mutex);
        }
      else
        {
          g_mutex_unlock(self->suspend_mutex);

          g_mutex_lock(self->queue_mutex);
          if (log_queue_get_length(self->queue) == 0)
            {
              g_cond_wait(self->worker_thread_wakeup_cond, self->queue_mutex);
            }
          g_mutex_unlock(self->queue_mutex);
        }

      if (self->worker_thread_terminate)
        break;

      if (!log_threaded_dest_driver_worker_job(self))
        {
          if (self->worker_job_error)
            self->worker_job_error(self);
          log_threaded_dest_driver_worker_suspend(self);
        }
    }

  if (self->worker_thread_deinit_method)
    self->worker_thread_deinit_method(self);

  msg_debug("Worker thread finished",
            evt_tag_str("driver", self->super.super.id),
            NULL);

  return NULL;
}
