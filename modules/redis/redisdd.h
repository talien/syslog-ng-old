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

#ifndef REDISDD_H_INCLUDED
#define REDISDD_H_INCLUDED

#include "driver.h"
#include "value-pairs.h"

LogDriver *redis_dd_new(void);

void redis_dd_set_host(LogDriver *d, const gchar *host);
void redis_dd_set_port(LogDriver *d, gint port);
void redis_dd_set_value_pairs(LogDriver *d, ValuePairs *vp);
void redis_dd_set_command(LogDriver *d, const gchar *cmd);
void redis_dd_set_prefix(LogDriver *d, const gchar *p);

#endif
