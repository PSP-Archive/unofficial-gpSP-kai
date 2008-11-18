/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2007 takka <takka@tfact.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef CHEATS_H
#define CHEATS_H

#define CHEAT_NAME_LENGTH (40*4)
#define MAX_CHEATS_PAGE 3
#define MAX_CHEATS (MAX_CHEATS_PAGE * 10)

typedef enum
{
  CHEAT_TYPE_GAMESHARK_V1,
  CHEAT_TYPE_GAMESHARK_V3,
  CHEAT_TYPE_DIRECT_V1,
  CHEAT_TYPE_DIRECT_V3,
  CHEAT_TYPE_INVALID
} cheat_variant_t;

typedef struct
{
  char cheat_name[CHEAT_NAME_LENGTH];
  u32 cheat_active;
  u32 cheat_codes[256]; /* TODO */
  u32 num_cheat_lines;
  cheat_variant_t cheat_variant;
} cheat_t;

// グローバル変数宣言

extern u32 g_cheats_num;

// 関数宣言

void process_cheats();
void add_cheats(char *cheats_filename);

#endif
