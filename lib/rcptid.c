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
#include "rcptid.h"
#include "messages.h"
#include "logmsg.h"

struct _rcptcfg{
    PersistState *state;
    PersistEntryHandle handle;
};

static GStaticMutex rcptid_mutex = G_STATIC_MUTEX_INIT;

static struct _rcptcfg g_rcptcfg;

RcptidState g_rcptidstate;

static NameValueContainer*
state_handler_rcptid_dump_state(StateHandler *self)
{
  NameValueContainer *result = name_value_container_new();
  RcptidState *state = (RcptidState *) state_handler_get_state(self);
  if (state)
    {   
      name_value_container_add_int(result, "version", state->super.version);
      name_value_container_add_boolean(result, "big_endian",
          state->super.big_endian);
      name_value_container_add_int64(result, "rcptid", state->g_rcptid);
    }   
  state_handler_put_state(self);
  return result;
}

StateHandler*
create_state_handler_rcptid(PersistState *persist_state, const gchar *name)
{
  return create_state_handler(persist_state, name, state_handler_rcptid_dump_state);
}

void state_handler_register_rcptid_constructor()
{
  state_handler_register_constructor(RPCTID_PREFIX,
      create_state_handler_rcptid);
}

PersistState *
rcptcfg_get_persist_state(void)
{
  return g_rcptcfg.state;
}

PersistEntryHandle
rcptcfg_get_persist_handle(void)
{
  return g_rcptcfg.handle;
}

void
rcptcfg_set_persist_state(PersistState *state)
{
  g_rcptcfg.state = state;
}

void
rcptcfg_set_persist_handle(PersistEntryHandle handle)
{
  g_rcptcfg.handle = handle;
}

static gboolean
rcptid_restore_entry(PersistState *state, RcptidState *data)
{
  data = persist_state_map_entry(rcptcfg_get_persist_state(),rcptcfg_get_persist_handle());
  if (data->super.version > 0)
  {
     msg_error("Internal error restoring log reader state, stored data is too new",
              evt_tag_int("version", data->super.version),
              NULL);
     return FALSE;
  }
  else
  {
    g_rcptidstate.super.version = 0;
    if ((data->super.big_endian && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
        (!data->super.big_endian && G_BYTE_ORDER == G_BIG_ENDIAN))
    {
      data->super.big_endian = !data->super.big_endian;
      data->g_rcptid = GUINT64_SWAP_LE_BE(data->g_rcptid);
    }
    g_rcptidstate.super.big_endian = data->super.big_endian;
    g_rcptidstate.g_rcptid = data->g_rcptid;
  }
  persist_state_unmap_entry(rcptcfg_get_persist_state(),rcptcfg_get_persist_handle());
  return TRUE;
}

static
gboolean rcptid_create_new_entry(PersistState *state, RcptidState *data)
{
  rcptcfg_set_persist_handle(persist_state_alloc_entry(state, "next.rcptid", sizeof(RcptidState)));
  data = persist_state_map_entry(rcptcfg_get_persist_state(),rcptcfg_get_persist_handle());
  data->super.version = g_rcptidstate.super.version = 0;
  data->super.big_endian = g_rcptidstate.super.big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
  data->g_rcptid = g_rcptidstate.g_rcptid = 1;
  persist_state_unmap_entry(rcptcfg_get_persist_state(),rcptcfg_get_persist_handle());
  return TRUE;
}

/****************************************************
* Generate a unique receipt ID ($RCPTID)
****************************************************/
/*restore RCTPID from persist file, if possible, else
create new enrty point with "next.rcptid" name*/
gboolean
rcptid_init(PersistState *state)
{
  RcptidState *data;
  gsize size;
  guint8 version;

  rcptcfg_set_persist_state(state);

  rcptcfg_set_persist_handle(persist_state_lookup_entry(state, "next.rcptid", &size, &version));
  if (rcptcfg_get_persist_handle())
  {
    return rcptid_restore_entry(state, data);
  }
  else
  {
    return rcptid_create_new_entry(state, data);
  }

}

void
log_msg_create_rcptid(LogMessage *msg)
{
  if (rcptcfg_get_persist_state())
  {
    g_static_mutex_lock(&rcptid_mutex);

    RcptidState *data;

    msg->rcptid = g_rcptidstate.g_rcptid++;

    if (!(g_rcptidstate.g_rcptid&=0xFFFFFFFFFFFF))
      ++g_rcptidstate.g_rcptid;

    data = persist_state_map_entry(rcptcfg_get_persist_state(),rcptcfg_get_persist_handle());
    data->g_rcptid = g_rcptidstate.g_rcptid;
    persist_state_unmap_entry(rcptcfg_get_persist_state(),rcptcfg_get_persist_handle());

    g_static_mutex_unlock(&rcptid_mutex);
  }
}
