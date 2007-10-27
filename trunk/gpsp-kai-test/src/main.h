/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2007 takka <takka@tfact.net>
 * Copyright (C) 2007 ????? <?????>
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
 * main.h
 * メインヘッダ
 ******************************************************************************/
#ifndef MAIN_H
#define MAIN_H

/******************************************************************************
 * マクロ等の定義
 ******************************************************************************/
typedef enum
{
  TIMER_INACTIVE,
  TIMER_PRESCALE,
  TIMER_CASCADE
} TIMER_STATUS_TYPE;

typedef enum
{
  TIMER_NO_IRQ,
  TIMER_TRIGGER_IRQ
} TIMER_IRQ_TYPE;

typedef enum
{
  TIMER_DS_CHANNEL_NONE,
  TIMER_DS_CHANNEL_A,
  TIMER_DS_CHANNEL_B,
  TIMER_DS_CHANNEL_BOTH
} TIMER_DS_CHANNEL_TYPE;

typedef struct
{
  s32 count;
  u32 reload;
  u32 prescale;
  u32 stop_cpu_ticks; /* NOT USE */
  FIXED16_16 frequency_step;
  TIMER_DS_CHANNEL_TYPE direct_sound_channels;
  TIMER_IRQ_TYPE irq;
  TIMER_STATUS_TYPE status;
} TIMER_TYPE;

typedef enum
{
  auto_frameskip,
  manual_frameskip,
  no_frameskip
} frameskip_type;

// PSPの種類
typedef enum
{
  psp_1000,      /* PSP-1000 all CFW */
  psp_2000_old,  /* PSP-2000 CFW 3.60 */
  psp_2000_new   /* PSP-2000 CFW 3.71 or higher */
} MODEL_TYPE;

// TODO:タイマーカウンタ周りの処理は再検討

// タイマーリロード時のカウンタの設定(この時点ではタイマーにセットされない)
// タイマースタート時にカウンタに設定される
// ただし、32bitアクセス時には即座にタイマーにセットされる
// 実機では0~0xFFFFだが、gpSP内部では (0x10000~1)<<prescale(0,6,8,10)の値をとる
// 各カウンターにリロードする際にprescale分シフトされる
// TODO:32bitアクセスと8/16bitアクセスで処理を分ける必要がある
// 8/16ビットアクセス時には呼び出す必要がない？
#define COUNT_TIMER(timer_number)                                             \
  timer[timer_number].reload = 0x10000 - value;                               \
  if(timer_number < 2)                                                        \
  {                                                                           \
    u32 timer_reload =                                                        \
     timer[timer_number].reload;              \
    SOUND_UPDATE_FREQUENCY_STEP(timer_number);                                \
  }                                                                           \

// タイマーの値の調整
// TODO:調整必要
#define ADJUST_SOUND_BUFFER(timer_number, channel)                            \
  if(timer[timer_number].direct_sound_channels & (0x01 << channel))           \
  {                                                                           \
    direct_sound_channel[channel].buffer_index = gbc_sound_buffer_index;      \
  }                                                                           \

// タイマーのアクセスとカウント開始処理
#define TRIGGER_TIMER(timer_number)                                           \
  if(value & 0x80)                                                            \
  {                                                                           \
    /* スタートビットが”1”だった場合 */                                     \
    if(timer[timer_number].status == TIMER_INACTIVE)                          \
    {                                                                         \
      /* タイマーが停止していた場合 */                                        \
      /* 各種設定をして、タイマー作動 */                                      \
                                                                              \
      /* リロード値を読み込む */                                              \
      u32 timer_reload = timer[timer_number].reload;                          \
                                                                              \
      /* カスケードモードか判別(タイマー0以外)*/                              \
      if(((value >> 2) & 0x01) && (timer_number != 0))                        \
      {                                                                       \
        /* カスケードモード */                                                \
        timer[timer_number].status = TIMER_CASCADE;                           \
        /* プリスケールの設定 */                                              \
        timer[timer_number].prescale = 0;                                     \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        /* プリスケールモード */                                              \
        timer[timer_number].status = TIMER_PRESCALE;                          \
        u32 prescale = prescale_table[value & 0x03];                          \
        /* プリスケールの設定 */                                              \
        timer[timer_number].prescale = prescale;                              \
      }                                                                       \
                                                                              \
      /* IRQの設定 */                                                         \
      timer[timer_number].irq = (value >> 6) & 0x01;                          \
                                                                              \
      /* カウンタを設定 */                                                    \
      timer[timer_number].count = timer_reload << timer[timer_number].prescale; \
      ADDRESS16(io_registers, 0x100 + (timer_number * 4)) =                   \
      0x10000 - timer_reload;                                                 \
                                                                              \
      if(timer[timer_number].count < execute_cycles)                          \
        execute_cycles = timer[timer_number].count;                           \
                                                                              \
      if(timer_number < 2)                                                    \
      {                                                                       \
        /* 小数点以下を切り捨てていたので、GBCサウンドと同様の処理にした*/   \
        SOUND_UPDATE_FREQUENCY_STEP(timer_number);                            \
        ADJUST_SOUND_BUFFER(timer_number, 0);                                 \
        ADJUST_SOUND_BUFFER(timer_number, 1);                                 \
      }                                                                       \
    }                                                                         \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    if(timer[timer_number].status != TIMER_INACTIVE)                          \
    {                                                                         \
      timer[timer_number].status = TIMER_INACTIVE;                            \
    }                                                                         \
  }                                                                           \
  ADDRESS16(io_registers, 0x102 + (timer_number * 4)) = value;                \

/******************************************************************************
 * グローバル変数の宣言
 ******************************************************************************/
extern u32 cpu_ticks;
extern u32 frame_ticks;
extern u32 execute_cycles;
extern u32 global_cycles_per_instruction;
extern u32 synchronize_flag;
extern u32 skip_next_frame_flag;
extern TIMER_TYPE timer[4];
extern u32 prescale_table[];
extern u32 cycle_memory_access;
extern u32 cycle_pc_relative_access;
extern u32 cycle_sp_relative_access;
extern u32 cycle_block_memory_access;
extern u32 cycle_block_memory_sp_access;
extern u32 cycle_block_memory_words;
extern u32 cycle_dma16_words;
extern u32 cycle_dma32_words;
extern u32 flush_ram_count;
extern u64 base_timestamp;
extern char main_path[MAX_PATH];
extern char rom_path[MAX_PATH];
extern u32 hold_state;
extern vu32 quit_flag;
extern vu32 power_flag;
extern volatile u32 real_frame_count;
extern u32 virtual_frame_count;
extern u32 max_frameskip;
extern u32 num_skipped_frames;
extern u64 frame_count_initial_timestamp;
extern int date_format;
extern MODEL_TYPE psp_model;

/******************************************************************************
 * グローバル関数の宣言
 ******************************************************************************/
void set_cpu_clock(u32 clock);
u32 update_gba();
void reset_gba();
void synchronize();
void quit();
void delay_us(u32 us_count);
void get_ticks_us(u64 *tick_return);
void game_name_ext(u8 *src, u8 *buffer, u8 *extension);
void main_read_mem_savestate(FILE_TAG_TYPE savestate_file);
void main_write_mem_savestate(FILE_TAG_TYPE savestate_file);
void main_read_savestate(FILE_TAG_TYPE savestate_file);
void error_msg(char *text);
void set_cpu_mode(CPU_MODE_TYPE new_mode);
void raise_interrupt(irq_type irq_raised);
void change_ext(char *src, char *buffer, char *extension);
u32 file_length(char *filename, s32 dummy);
MODEL_TYPE get_model();

#endif /* MAIN_H */

