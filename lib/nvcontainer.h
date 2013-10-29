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
#ifndef _NVCONTAINER_H
#define _NVCONTAINER_H
#include "syslog-ng.h"

typedef struct _NameValueContainer NameValueContainer;

NameValueContainer *name_value_container_new();

gboolean name_value_container_parse_json_string(NameValueContainer *self, gchar *json_string);

void name_value_container_add_string(NameValueContainer *self, gchar *name, gchar *value);
void name_value_container_add_boolean(NameValueContainer *self, gchar *name, gboolean value);
void name_value_container_add_int(NameValueContainer *self, gchar *name, gint value);
void name_value_container_add_int64(NameValueContainer *self, gchar *name, gint64 value);
gchar *name_value_container_get_value(NameValueContainer *self, gchar *name);

const gchar *name_value_container_get_string(NameValueContainer *self, gchar *name);
gboolean name_value_container_get_boolean(NameValueContainer *self, gchar *name);
gint name_value_container_get_int(NameValueContainer *self, gchar *name);
gint64 name_value_container_get_int64(NameValueContainer *self, gchar *name);

gchar *name_value_container_get_json_string(NameValueContainer *self);
void name_value_container_foreach(NameValueContainer *self, void
(callback)(gchar *name, const gchar *value, gpointer user_data), gpointer user_data);

gboolean  name_value_container_does_name_exist(NameValueContainer *self, gchar *name);
void name_value_container_free(NameValueContainer *self);

#endif

