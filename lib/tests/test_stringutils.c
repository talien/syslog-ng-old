/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "testutils.h"
#include "stringutils.h"
#include <stdio.h>

static void
replace_string_testcase(const gchar *string, const gchar *pattern, const gchar *replacement, const gchar *expected)
{
  gchar *result = replace_string(string, pattern, replacement);
  assert_string(result, expected, "Failed replacement string: %s; pattern: %s; replacement: %s", string, pattern, replacement);
  g_free(result);
}


void test_replace_string()
{
  replace_string_testcase("hello", "xxx", "zzz", "hello");
  replace_string_testcase("hello", "llo" ,"xx", "hexx");
  replace_string_testcase("hello",  "hel", "xx", "xxlo");
  replace_string_testcase("c:\\var\\c.txt", "\\", "\\\\", "c:\\\\var\\\\c.txt");
  replace_string_testcase("c:\\xxx\\xxx", "xxx", "var", "c:\\var\\var");
}

void
test_data_to_hex()
{
  guint8 input[] = {0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,0x10,0x20,0xff,0x00};
  gchar *expected_output = "01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 20 FF";
  gchar *output = data_to_hex_string(input,strlen(input));
  assert_string(output,expected_output,"Bad working!\n");
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  test_data_to_hex();
  test_replace_string();
  return 0;
}
