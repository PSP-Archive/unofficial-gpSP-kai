/* unofficial gameplaySP kai
 *
 * Copyright (C) 2007 NJ
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

/******************************************************************************
 * draw.h
 * 基本描画の処理
 ******************************************************************************/
#ifndef DRAW_H
#define DRAW_H

/******************************************************************************
 * マクロ等の定義
 ******************************************************************************/
#define COLOR16(red, green, blue) ((blue << 10) | (green << 5) | red)
#define GET_R16(color) (color & 0x1f)
#define GET_G16(color) ((color >> 5) & 0x1f)
#define GET_B16(color) ((color >> 10)& 0x1f)
#define COLOR32(red, green, blue) (0xff000000 | ((blue & 0xff) << 16) | ((green & 0xff) << 8) | (red & 0xff))

#define PRINT_STRING_EXT_BG(str, fg_color, bg_color, x, y, dest_ptr, pitch)                                            \
  fbm_printVRAM( dest_ptr, pitch, x, y, str, fg_color, bg_color, FBM_FONT_FILL | FBM_BACK_FILL)                        \

#define PRINT_STRING(str, fg_color, x, y)                                                                              \
  fbm_printVRAM( screen_address, screen_pitch, x, y, str, fg_color, 0, FBM_FONT_FILL)                                  \

#define PRINT_STRING_BG(str, fg_color, bg_color, x, y)                                                                 \
  fbm_printVRAM( screen_address, screen_pitch, x, y, str, fg_color, bg_color, FBM_FONT_FILL | FBM_BACK_FILL)           \

#define PRINT_STRING_SHADOW(str, fg_color, x, y)                                                                       \
  fbm_printVRAM( screen_address, screen_pitch, x + 1, y + 1, str, 0, 0, FBM_FONT_FILL);                                \
  fbm_printVRAM( screen_address, screen_pitch, x, y, str, fg_color, 0, FBM_FONT_FILL)                                  \

#define PRINT_STRING_BG_SJIS(utf8, str, fg_color, bg_color, x, y)                                                      \
  sjis_to_utf8(str, utf8);                                                                                             \
  fbm_printVRAM( screen_address, screen_pitch, x, y, utf8, fg_color, bg_color, FBM_FONT_FILL | FBM_BACK_FILL)          \


// 基本カラーの設定
#define COLOR_WHITE         COLOR16(31, 31, 31)
#define COLOR_BLACK         COLOR16( 0,  0,  0)
#define COLOR_TEXT          COLOR16(31, 31, 31)
#define COLOR_PROGRESS_TEXT COLOR16( 0,  0,  0)
#define COLOR_PROGRESS_BAR  COLOR16(15, 15, 15)
#define COLOR_ERROR         COLOR16(31,  0,  0)
#define COLOR_BG            COLOR16(2,  4,  10)
#define COLOR_BG32          COLOR32(2*8,  4*8,  10*8)
#define COLOR_ROM_INFO      COLOR16(22, 18, 26)
#define COLOR_ACTIVE_ITEM   COLOR16(31, 31, 31)
#define COLOR_INACTIVE_ITEM COLOR16(13, 20, 18)
#define COLOR_HELP_TEXT     COLOR16(16, 20, 24)
#define COLOR_DIALOG        COLOR16(31, 31, 31)
#define COLOR_DIALOG_SHADOW COLOR16( 0,  2,  8)
#define COLOR_FRAME         COLOR16( 0,  0,  0)
#define COLOR_YESNO_TEXT    COLOR16( 0,  0,  0)
/******************************************************************************
 * グローバル変数の宣言
 ******************************************************************************/

/******************************************************************************
 * グローバル関数の宣言
 ******************************************************************************/
extern void print_string_center(u32 sy, u32 color, u32 bg_color, char *str);
extern void print_string_shadow_center(u32 sy, u32 color, char *str);
extern void hline(u32 sx, u32 ex, u32 y, u32 color);
extern void hline_alpha(u32 sx, u32 ex, u32 y, u32 color, u32 alpha);
extern void vline(u32 x, u32 sy, u32 ey, u32 color);
extern void vline_alpha(u32 x, u32 sy, u32 ey, u32 color, u32 alpha);
extern void box(u32 sx, u32 sy, u32 ex, u32 ey, u32 color);
extern void boxfill(u32 sx, u32 sy, u32 ex, u32 ey, u32 color);
extern void boxfill_alpha(u32 sx, u32 sy, u32 ex, u32 ey, u32 color, u32 alpha);
extern void init_progress(u32 total, char *text);
extern void update_progress(void);
extern void show_progress(char *text);
extern void scrollbar(u32 sx, u32 sy, u32 ex, u32 ey, u32 all,u32 view,u32 now);
extern u32 yesno_dialog(char *text);
extern void msg_screen_init(const char *title);
extern void msg_screen_draw();
extern void msg_printf(const char *text, ...);
extern void msg_screen_clear(void);
extern void msg_set_text_color(u32 color);

#endif

