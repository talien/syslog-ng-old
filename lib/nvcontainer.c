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
#include "nvcontainer.h"
#include "json.h"

struct _NameValueContainer
{
  struct json_object *container;
};

NameValueContainer *
name_value_container_new()
{
  NameValueContainer *self = g_new0(NameValueContainer, 1); 
  self->container = json_object_new_object();
  return self;
}

gboolean
name_value_container_parse_json_string(NameValueContainer *self, gchar *json_string)
{
  struct json_object *new_container = json_tokener_parse(json_string);
  if (!new_container)
    {   
      return FALSE;
    }   
  json_object_put(self->container);
  self->container = new_container;
  return TRUE;
}

void
name_value_container_add_string(NameValueContainer *self, gchar *name,
    gchar *value)
{
  json_object_object_add(self->container, name, json_object_new_string(value));
}

void
name_value_container_add_boolean(NameValueContainer *self, gchar *name,
    gboolean value)
{
  json_object_object_add(self->container, name, json_object_new_boolean(value));
}

void
name_value_container_add_int(NameValueContainer *self, gchar *name, gint value)
{
  json_object_object_add(self->container, name, json_object_new_int(value));
}
void
name_value_container_add_int64(NameValueContainer *self, gchar *name,
    gint64 value)
{
  json_object_object_add(self->container, name, json_object_new_int64(value));
}

gboolean
name_value_container_does_name_exist(NameValueContainer *self, gchar *name)
{
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  return !!(value);
}

gchar *
name_value_container_get_value(NameValueContainer *self, gchar *name)
{
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value)
    {
      return g_strdup(json_object_get_string(value));
    }
  return NULL;
}

const gchar *
name_value_container_get_string(NameValueContainer *self, gchar *name)
{
  const gchar *result = NULL;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_string)))
    {
      result = json_object_get_string(value);
    }
  return result;
}

gboolean
name_value_container_get_boolean(NameValueContainer *self, gchar *name)
{
  gboolean result = FALSE;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_boolean)))
    {
      result = json_object_get_boolean(value);
    }
  return result;
}

gint
name_value_container_get_int(NameValueContainer *self, gchar *name)
{
  gint result = -1;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_int)))
    {
      result = json_object_get_int(value);
    }
  return result;
}

gint64
name_value_container_get_int64(NameValueContainer *self, gchar *name)
{
  gint64 result = FALSE;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_int)))
    {
      result = json_object_get_int64(value);
    }
  return result;
}

void
name_value_container_foreach(NameValueContainer *self, void
(callback)(gchar *name, const gchar *value, gpointer user_data), gpointer user_data)
{

  json_object_object_foreach(self->container, key, val)
    {
      callback(key, json_object_get_string(val), user_data);
    }
  return;
}

gchar *
name_value_container_get_json_string(NameValueContainer *self)
{
  return g_strdup(json_object_to_json_string(self->container));
}

void
name_value_container_free(NameValueContainer *self)
{
  json_object_put(self->container);
  g_free(self);
}

