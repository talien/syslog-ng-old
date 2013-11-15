/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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
#include <logpipe.h>
#include "assert-pipe.h"
#include "testutils.h"

typedef struct _AssertPipe {
   LogPipe super;
   GList* asserts;
   GList* current_assert;
} AssertPipe;

static char* 
assert_pipe_get_next_value(AssertPipe* self)
{
   char* value = self->current_assert->data;
   self->current_assert = g_list_next(self->current_assert);
   return value;
};

static int
assert_pipe_has_more_items(AssertPipe* self)
{
   return self->current_assert != NULL;
};

static void assert_pipe_queue(LogPipe* s, LogMessage* lm, const LogPathOptions *path_options, gpointer user_data)
{
   AssertPipe* self = (AssertPipe*) s;
   gssize value_len;
   const char *expected_name, *expected_value, *actual_value;

   assert_true(assert_pipe_has_more_items(self), "AssertPipe has no more items to assert!");
   expected_name = assert_pipe_get_next_value(self);
   expected_value = assert_pipe_get_next_value(self);
   NVHandle handle = log_msg_get_value_handle(expected_name);
   assert_true(handle != 0, "AssertPipe: no such value in this message!");
   actual_value = log_msg_get_value(lm, handle, &value_len);
   assert_string(expected_value, actual_value, "AssertPipe: Expected message value does not equals to actual one!");
};

void assert_pipe_init(AssertPipe* self, gchar* assert_key, gchar* assert_value, va_list va)
{ 
   char* key;
   char* value;
   self->super.queue = assert_pipe_queue;
   
   key = assert_key;
   value = assert_value;
   while (key)
   {
      self->asserts = g_list_append(self->asserts, key);
      self->asserts = g_list_append(self->asserts, value);
      key = va_arg(va, char*);
      value = va_arg(va, char*);
   };
   self->current_assert = self->asserts;
   
};

LogPipe* assert_pipe_new(char* assert_key, char* assert_value, ...)
{
   AssertPipe* self = g_new0(AssertPipe, 1);
   log_pipe_init_instance(&self->super);
   va_list va;
   va_start(va, assert_value);
   assert_pipe_init(self, assert_key, assert_value, va);
   va_end(va);
   return &self->super;
};


