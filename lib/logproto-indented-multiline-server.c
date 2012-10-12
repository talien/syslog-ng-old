/*
 * Copyright (c) 2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
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
 */
#include "logproto-indented-multiline-server.h"
#include "messages.h"

#include <stdio.h>

static gboolean
log_proto_indented_multiline_server_line_is_complete(LogProtoTextServer *self,
                                                     LogProtoBufferedServerState *state,
                                                     const guchar **eol)
{
  gsize pos;

  if (!*eol)
    return FALSE;

  /* If we have eol, find more eols until we get one where the next
     char is non-whitespace. */

  pos = *eol - self->super.buffer;

  while (pos < state->pending_buffer_end - 1)
    {
      if (self->super.buffer[pos] == '\n')
        *eol = self->super.buffer + pos;
      else
        {
          pos++;
          continue;
        }

      if (self->super.buffer[pos + 1] != ' ' &&
          self->super.buffer[pos + 1] != '\t')
        return TRUE;

      pos++;
    }

  return FALSE;
}

static void
log_proto_indented_multiline_server_line_flush (LogProtoTextServer *self,
                                                LogProtoBufferedServerState *state,
                                                const guchar **msg, gsize *msg_len,
                                                const guchar *buffer_start, gsize buffer_bytes)
{
  *msg = buffer_start;
  *msg_len = buffer_bytes;
  if ((*msg)[buffer_bytes - 1] == '\n')
    (*msg_len)--;

  state->pending_buffer_pos = state->pending_buffer_end;
}

void
log_proto_indented_multiline_server_init(LogProtoIMultiLineServer *self,
                                         LogTransport *transport,
                                         const LogProtoServerOptions *options)
{
  log_proto_text_server_init(&self->super, transport, options);
  self->super.is_line_complete = log_proto_indented_multiline_server_line_is_complete;
  self->super.line_flush = log_proto_indented_multiline_server_line_flush;
}

LogProtoServer *
log_proto_indented_multiline_server_new(LogTransport *transport, const LogProtoServerOptions *options)
{
  LogProtoIMultiLineServer *self = g_new0(LogProtoIMultiLineServer, 1);

  log_proto_indented_multiline_server_init(self, transport, options);
  return &self->super.super.super;
}
