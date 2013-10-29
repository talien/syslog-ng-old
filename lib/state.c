/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013 Viktor Juhasz
 * Copyright (c) 2013 Viktor Tusa
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
#include "state.h"
#include "misc.h"
#include "logmsg.h"
#include "stringutils.h"

static GHashTable *persist_state_storage = NULL;

gboolean
state_handler_load_state(StateHandler *self, NameValueContainer *new_state, GError **error)
{
  if (!self->load_state)
    {
      if (error)
        *error = g_error_new(1, 1, "load_state isn't implemented for this state_handler");

      return FALSE;
    }
  return self->load_state(self, new_state, error);
}

PersistEntryHandle
state_handler_get_handle(StateHandler *self)
{
  PersistEntryHandle result = 0;
  if (self->persist_handle == 0)
    {
      self->persist_handle = persist_state_lookup_entry(self->persist_state,
          self->name, &self->size, &self->version);
    }
  result = self->persist_handle;
  return result;
}

gboolean
state_handler_entry_exists(StateHandler *self)
{
  return !!(persist_state_lookup_entry(self->persist_state, self->name,
      &self->size, &self->version));
}

gchar *
state_handler_get_persist_name(StateHandler *self)
{
  return self->name;
}

guint8
state_handler_get_persist_version(StateHandler *self)
{
  return self->version;
}

gpointer
state_handler_get_state(StateHandler *self)
{
  if (self->state)
    return self->state;

  if (self->persist_handle == 0)
    self->persist_handle = state_handler_get_handle(self);
  if (self->persist_handle)
    self->state = persist_state_map_entry(self->persist_state, self->persist_handle);

  return self->state;
}

PersistEntryHandle
state_handler_alloc_state(StateHandler *self)
{
  state_handler_put_state(self);
  self->persist_handle = 0;
  if (self->alloc_state)
    {
      self->persist_handle = self->alloc_state(self);
    }
  return self->persist_handle;
}

void
state_handler_put_state(StateHandler *self)
{
  if (self->persist_state && self->persist_handle && self->state != NULL)
    {
      persist_state_unmap_entry(self->persist_state, self->persist_handle);
    }
  self->state = NULL;
}

gboolean
state_handler_rename_entry(StateHandler *self, const gchar *new_name)
{
  state_handler_put_state(self);
  if (persist_state_rename_entry(self->persist_state, self->name, new_name))
    {
      g_free(self->name);
      self->name = g_strdup(new_name);
      return TRUE;
    }
  return FALSE;
}

NameValueContainer *
state_handler_dump_state(StateHandler *self)
{
  if (!self->dump_state)
    {
      return NULL;
    }
  return self->dump_state(self);
}

void
state_handler_free(StateHandler *self)
{
  if (self == NULL)
    {
      return;
    }
  state_handler_put_state(self);
  if (self->free_fn)
    {
      self->free_fn(self);
    }
  g_free(self->name);
  g_free(self);
}

void
state_handler_init(StateHandler *self, PersistState *persist_state,
    const gchar *name)
{
  self->persist_state = persist_state;
  self->name = g_strdup(name);
}

static NameValueContainer*
default_state_handler_dump_state(StateHandler *self)
{
  NameValueContainer *result = name_value_container_new();
  state_handler_get_state(self);
  if (self->state)
    {
      gchar *value = data_to_hex_string((gpointer) self->state, (guint32) self->size);
      name_value_container_add_string(result, "value", value);
      g_free(value);
    }
  state_handler_put_state(self);
  return result;
}

StateHandler*
create_state_handler(PersistState *persist_state, const gchar* name, NameValueContainer*(*dump_state_callback)(StateHandler *self))
{
  StateHandler *self = g_new0(StateHandler, 1);
  state_handler_init(self, persist_state, name);
  self->dump_state = dump_state_callback;
  return self;
 
};

StateHandler*
create_default_state_handler(PersistState *persist_state, const gchar *name)
{
  return create_state_handler(persist_state, name, default_state_handler_dump_state);
}

STATE_HANDLER_CONSTRUCTOR
state_handler_get_constructor_by_prefix(const gchar *prefix)
{
  STATE_HANDLER_CONSTRUCTOR result = NULL;
  if (persist_state_storage)
    {
      result = g_hash_table_lookup(persist_state_storage, prefix);
    }
  if (!result)
    {
      result = create_default_state_handler;
    }
  return result;
}

void
state_handler_register_constructor(const gchar *prefix,
    STATE_HANDLER_CONSTRUCTOR handler)
{
  if (!persist_state_storage)
    {
      persist_state_storage = g_hash_table_new(g_str_hash, g_str_equal);
    }
  g_hash_table_insert(persist_state_storage, (gpointer) prefix, handler);
}
