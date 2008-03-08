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

#include "common.h"


#define BUFFER_SIZE 65536
#define SOUND_BUFFER_SIZE 2048

DIRECT_SOUND_STRUCT direct_sound_channel[2];
GBC_SOUND_STRUCT gbc_sound_channel[4];

u32 global_enable_audio = 1;
u8 sound_on = 0;
int sound_volume;


static void init_noise_table(u32 *table, u32 period, u32 bit_length);

static SceUID sound_thread;
static int sound_handle;
static int sound_update_thread(SceSize args, void *argp);
static void sound_callback(s16 *stream, int length);

static u8 sound_active;

static s16 psp_sound_buffer[SOUND_BUFFER_SIZE];

static s16 sound_buffer[BUFFER_SIZE];
static u32 sound_buffer_base = 0;

static u32 sound_last_cpu_ticks = 0;
static FIXED16_16 gbc_sound_tick_step;


void sound_timer_queue32(u8 channel)
{
  DIRECT_SOUND_STRUCT *ds = direct_sound_channel + channel;
  u8 offset = channel * 4;
  u8 i;

  for(i = 0xA0; i <= 0xA3; i++)
  {
    ds->fifo[ds->fifo_top] = ADDRESS8(io_registers, i + offset);
    ds->fifo_top = (ds->fifo_top + 1) % 32;
  }
}

// Unqueue 1 sample from the base of the DS FIFO and place it on the audio
// buffer for as many samples as necessary. If the DS FIFO is 16 bytes or
// smaller and if DMA is enabled for the sound channel initiate a DMA transfer
// to the DS FIFO.

#define render_sample_null()                                                  \

#define render_sample_right()                                                 \
  sound_buffer[buffer_index + 1] += current_sample +                          \
   FP16_16_TO_U32((next_sample - current_sample) * fifo_fractional)           \

#define render_sample_left()                                                  \
  sound_buffer[buffer_index] += current_sample +                              \
   FP16_16_TO_U32((next_sample - current_sample) * fifo_fractional)           \

#define render_sample_both()                                                  \
  dest_sample = current_sample +                                              \
   FP16_16_TO_U32((next_sample - current_sample) * fifo_fractional);          \
  sound_buffer[buffer_index] += dest_sample;                                  \
  sound_buffer[buffer_index + 1] += dest_sample                               \

#define render_samples(type)                                                  \
  while(fifo_fractional <= 0xFFFF)                                            \
  {                                                                           \
    render_sample_##type();                                                   \
    fifo_fractional += frequency_step;                                        \
    buffer_index = (buffer_index + 2) % BUFFER_SIZE;                          \
  }                                                                           \

void sound_timer(FIXED16_16 frequency_step, u8 channel)
{
  DIRECT_SOUND_STRUCT *ds = direct_sound_channel + channel;

  FIXED16_16 fifo_fractional = ds->fifo_fractional;
  u32 buffer_index = ds->buffer_index;
  s16 current_sample, next_sample, dest_sample;

  current_sample = ds->fifo[ds->fifo_base] << 4;
  ds->fifo_base = (ds->fifo_base + 1) % 32;
  next_sample = ds->fifo[ds->fifo_base] << 4;

  if(sound_on == 1)
  {
    if(ds->volume == DIRECT_SOUND_VOLUME_50)
    {
      current_sample >>= 1;
      next_sample >>= 1;
    }

    switch(ds->status)
    {
      case DIRECT_SOUND_INACTIVE:
        render_samples(null);
        break;

      case DIRECT_SOUND_RIGHT:
        render_samples(right);
        break;

      case DIRECT_SOUND_LEFT:
        render_samples(left);
        break;

      case DIRECT_SOUND_LEFTRIGHT:
        render_samples(both);
        break;
    }
  }
  else
  {
    render_samples(null);
  }

  ds->buffer_index = buffer_index;
  ds->fifo_fractional = FP16_16_FRACTIONAL_PART(fifo_fractional);

  u8 fifo_length;

  if(ds->fifo_top > ds->fifo_base)
    fifo_length = ds->fifo_top - ds->fifo_base;
  else
    fifo_length = ds->fifo_top + (32 - ds->fifo_base);

  if(fifo_length <= 16)
  {
    if(dma[1].direct_sound_channel == channel)
      dma_transfer(dma + 1);

    if(dma[2].direct_sound_channel == channel)
      dma_transfer(dma + 2);
  }
}

void sound_reset_fifo(u8 channel)
{
  DIRECT_SOUND_STRUCT *ds = direct_sound_channel + channel;

  memset(ds->fifo, 0, 32);
}

// Initial pattern data = 4bits (signed)
// Channel volume = 12bits
// Envelope volume = 14bits
// Master volume = 2bits

// Recalculate left and right volume as volume changes.
// To calculate the current sample, use (sample * volume) >> 16

// Square waves range from -8 (low) to 7 (high)

s8 square_pattern_duty[4][8] =
{
  { 0xF8, 0xF8, 0xF8, 0xF8, 0x07, 0xF8, 0xF8, 0xF8 },
  { 0xF8, 0xF8, 0xF8, 0xF8, 0x07, 0x07, 0xF8, 0xF8 },
  { 0xF8, 0xF8, 0x07, 0x07, 0x07, 0x07, 0xF8, 0xF8 },
  { 0x07, 0x07, 0x07, 0x07, 0xF8, 0xF8, 0x07, 0x07 }
};

static s8 wave_samples[64];

static u32 noise_table15[1024];
static u32 noise_table7[4];

static const u8 gbc_sound_master_volume_table[4] = { 1, 2, 4, 0 };

static u32 gbc_sound_channel_volume_table[8] =
{
  FIXED_DIV(0, 7, 12),
  FIXED_DIV(1, 7, 12),
  FIXED_DIV(2, 7, 12),
  FIXED_DIV(3, 7, 12),
  FIXED_DIV(4, 7, 12),
  FIXED_DIV(5, 7, 12),
  FIXED_DIV(6, 7, 12),
  FIXED_DIV(7, 7, 12)
};

static u32 gbc_sound_envelope_volume_table[16] =
{
  FIXED_DIV( 0, 15, 14),
  FIXED_DIV( 1, 15, 14),
  FIXED_DIV( 2, 15, 14),
  FIXED_DIV( 3, 15, 14),
  FIXED_DIV( 4, 15, 14),
  FIXED_DIV( 5, 15, 14),
  FIXED_DIV( 6, 15, 14),
  FIXED_DIV( 7, 15, 14),
  FIXED_DIV( 8, 15, 14),
  FIXED_DIV( 9, 15, 14),
  FIXED_DIV(10, 15, 14),
  FIXED_DIV(11, 15, 14),
  FIXED_DIV(12, 15, 14),
  FIXED_DIV(13, 15, 14),
  FIXED_DIV(14, 15, 14),
  FIXED_DIV(15, 15, 14)
};

u32 gbc_sound_buffer_index = 0;
static u32 gbc_sound_last_cpu_ticks = 0;
static u32 gbc_sound_partial_ticks = 0;

u8 gbc_sound_master_volume_left;
u8 gbc_sound_master_volume_right;
u8 gbc_sound_master_volume;

#define update_volume_channel_envelope(channel)                               \
  volume_##channel = gbc_sound_envelope_volume_table[envelope_volume] *       \
   gbc_sound_channel_volume_table[gbc_sound_master_volume_##channel] *        \
   gbc_sound_master_volume_table[gbc_sound_master_volume]                     \

#define update_volume_channel_noenvelope(channel)                             \
  volume_##channel = gs->wave_volume *                                        \
   gbc_sound_channel_volume_table[gbc_sound_master_volume_##channel] *        \
   gbc_sound_master_volume_table[gbc_sound_master_volume]                     \

#define update_volume(type)                                                   \
  update_volume_channel_##type(left);                                         \
  update_volume_channel_##type(right)                                         \

#define update_tone_sweep()                                                   \
  if(gs->sweep_status)                                                        \
  {                                                                           \
    u8 sweep_ticks = gs->sweep_ticks - 1;                                     \
                                                                              \
    if(sweep_ticks == 0)                                                      \
    {                                                                         \
      u16 rate = gs->rate;                                                    \
                                                                              \
      if(gs->sweep_direction)                                                 \
      {                                                                       \
        rate = rate - (rate >> gs->sweep_shift);                              \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        rate = rate + (rate >> gs->sweep_shift);                              \
      }                                                                       \
                                                                              \
      if(rate > 2047)                                                         \
      {                                                                       \
        gs->active_flag = 0;                                                  \
        break;                                                                \
      }                                                                       \
                                                                              \
      frequency_step = FLOAT_TO_FP16_16(((131072.0 / (2048 - rate)) * 8.0) /  \
       SOUND_FREQUENCY);                                                      \
                                                                              \
      gs->frequency_step = frequency_step;                                    \
      gs->rate = rate;                                                        \
                                                                              \
      sweep_ticks = gs->sweep_initial_ticks;                                  \
    }                                                                         \
    gs->sweep_ticks = sweep_ticks;                                            \
  }                                                                           \

#define update_tone_nosweep()                                                 \

#define update_tone_envelope()                                                \
  if(gs->envelope_status)                                                     \
  {                                                                           \
    u8 envelope_ticks = gs->envelope_ticks - 1;                               \
    envelope_volume = gs->envelope_volume;                                    \
                                                                              \
    if(envelope_ticks == 0)                                                   \
    {                                                                         \
      if(gs->envelope_direction)                                              \
      {                                                                       \
        if(envelope_volume != 15)                                             \
        {                                                                     \
          envelope_volume = gs->envelope_volume + 1;                          \
        }                                                                     \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        if(envelope_volume != 0)                                              \
        {                                                                     \
          envelope_volume = gs->envelope_volume - 1;                          \
        }                                                                     \
      }                                                                       \
                                                                              \
      update_volume(envelope);                                                \
                                                                              \
      gs->envelope_volume = envelope_volume;                                  \
      gs->envelope_ticks = gs->envelope_initial_ticks;                        \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      gs->envelope_ticks = envelope_ticks;                                    \
    }                                                                         \
  }                                                                           \

#define update_tone_noenvelope()                                              \

#define update_tone_counters(envelope_op, sweep_op)                           \
  tick_counter += gbc_sound_tick_step;                                        \
  if(tick_counter > 0xFFFF)                                                   \
  {                                                                           \
    tick_counter &= 0xFFFF;                                                   \
                                                                              \
    if(gs->length_status)                                                     \
    {                                                                         \
      u8 length_ticks = gs->length_ticks - 1;                                 \
      gs->length_ticks = length_ticks;                                        \
                                                                              \
      if(length_ticks == 0)                                                   \
      {                                                                       \
        gs->active_flag = 0;                                                  \
        break;                                                                \
      }                                                                       \
    }                                                                         \
                                                                              \
    update_tone_##envelope_op();                                              \
    update_tone_##sweep_op();                                                 \
  }                                                                           \

#define gbc_sound_render_sample_right()                                       \
  sound_buffer[buffer_index + 1] += (current_sample * volume_right) >> 22     \

#define gbc_sound_render_sample_left()                                        \
  sound_buffer[buffer_index] += (current_sample * volume_left) >> 22          \

#define gbc_sound_render_sample_both()                                        \
  gbc_sound_render_sample_right();                                            \
  gbc_sound_render_sample_left()                                              \

#define gbc_sound_render_samples(type, sample_length, envelope_op, sweep_op)  \
  for(i = 0; i < buffer_ticks; i++)                                           \
  {                                                                           \
    current_sample =                                                          \
     sample_data[FP16_16_TO_U32(sample_index) % sample_length];               \
    gbc_sound_render_sample_##type();                                         \
                                                                              \
    sample_index += frequency_step;                                           \
    buffer_index = (buffer_index + 2) % BUFFER_SIZE;                          \
                                                                              \
    update_tone_counters(envelope_op, sweep_op);                              \
  }                                                                           \

#define gbc_noise_wrap_full 32767

#define gbc_noise_wrap_half 126

#define get_noise_sample_full()                                               \
  current_sample =                                                            \
   ((s32)(noise_table15[FP16_16_TO_U32(sample_index) >> 5] <<                 \
   (FP16_16_TO_U32(sample_index) & 0x1F)) >> 31) & 0x0F                       \

#define get_noise_sample_half()                                               \
  current_sample =                                                            \
   ((s32)(noise_table7[FP16_16_TO_U32(sample_index) >> 5] <<                  \
   (FP16_16_TO_U32(sample_index) & 0x1F)) >> 31) & 0x0F                       \

#define gbc_sound_render_noise(type, noise_type, envelope_op, sweep_op)       \
  for(i = 0; i < buffer_ticks; i++)                                           \
  {                                                                           \
    get_noise_sample_##noise_type();                                          \
    gbc_sound_render_sample_##type();                                         \
                                                                              \
    sample_index += frequency_step;                                           \
                                                                              \
    if(sample_index >= U32_TO_FP16_16(gbc_noise_wrap_##noise_type))           \
    {                                                                         \
      sample_index -= U32_TO_FP16_16(gbc_noise_wrap_##noise_type);            \
    }                                                                         \
                                                                              \
    buffer_index = (buffer_index + 2) % BUFFER_SIZE;                          \
    update_tone_counters(envelope_op, sweep_op);                              \
  }                                                                           \

#define gbc_sound_render_channel(type, sample_length, envelope_op, sweep_op)  \
  buffer_index = gbc_sound_buffer_index;                                      \
  sample_index = gs->sample_index;                                            \
  frequency_step = gs->frequency_step;                                        \
  tick_counter = gs->tick_counter;                                            \
                                                                              \
  update_volume(envelope_op);                                                 \
                                                                              \
  switch(gs->status)                                                          \
  {                                                                           \
    case GBC_SOUND_INACTIVE:                                                  \
    {                                                                         \
      break;                                                                  \
    }                                                                         \
                                                                              \
    case GBC_SOUND_RIGHT:                                                     \
    {                                                                         \
      gbc_sound_render_##type(right, sample_length, envelope_op, sweep_op);   \
      break;                                                                  \
    }                                                                         \
                                                                              \
    case GBC_SOUND_LEFT:                                                      \
    {                                                                         \
      gbc_sound_render_##type(left, sample_length, envelope_op, sweep_op);    \
      break;                                                                  \
    }                                                                         \
                                                                              \
    case GBC_SOUND_LEFTRIGHT:                                                 \
    {                                                                         \
      gbc_sound_render_##type(both, sample_length, envelope_op, sweep_op);    \
      break;                                                                  \
    }                                                                         \
  }                                                                           \
                                                                              \
  gs->sample_index = sample_index;                                            \
  gs->tick_counter = tick_counter                                             \

#define gbc_sound_load_wave_ram(bank)                                         \
  wave_bank = wave_samples + (bank * 32);                                     \
  for(i = 0, i2 = 0; i < 16; i++, i2 += 2)                                    \
  {                                                                           \
    current_sample = wave_ram[i];                                             \
    wave_bank[i2] = (((current_sample >> 4) & 0x0F) - 8);                     \
    wave_bank[i2 + 1] = ((current_sample & 0x0F) - 8);                        \
  }                                                                           \

void update_gbc_sound(u32 cpu_ticks)
{
  FIXED16_16 buffer_ticks = FLOAT_TO_FP16_16(((float)
   (cpu_ticks - gbc_sound_last_cpu_ticks) * SOUND_FREQUENCY) / 16777216.0);
  u32 i, i2;
  GBC_SOUND_STRUCT *gs = gbc_sound_channel;
  FIXED16_16 sample_index, frequency_step;
  FIXED16_16 tick_counter;
  u32 buffer_index;
  s32 volume_left, volume_right;
  u8 envelope_volume;
  s32 current_sample;
  u16 sound_status = ADDRESS16(io_registers, 0x84) & 0xFFF0;
  s8 *sample_data;
  s8 *wave_bank;
  u8 *wave_ram = ((u8 *)io_registers) + 0x90;

  gbc_sound_partial_ticks += FP16_16_FRACTIONAL_PART(buffer_ticks);
  buffer_ticks = FP16_16_TO_U32(buffer_ticks);

  if(gbc_sound_partial_ticks > 0xFFFF)
  {
    buffer_ticks += 1;
    gbc_sound_partial_ticks &= 0xFFFF;
  }

  if(synchronize_flag)
  {
    if(((gbc_sound_buffer_index - sound_buffer_base) % BUFFER_SIZE) >
     (SOUND_BUFFER_SIZE * 3))
    {
      while(((gbc_sound_buffer_index - sound_buffer_base) % BUFFER_SIZE) >
       (SOUND_BUFFER_SIZE * 3))
      {
        sceKernelDelayThread(10);
      }

      if(current_frameskip_type == auto_frameskip)
      {
        sceDisplayWaitVblankStart();
        real_frame_count = 0;
        virtual_frame_count = 0;
      }
    }
  }

  if(sound_on == 1)
  {
    gs = gbc_sound_channel + 0;
    if(gs->active_flag)
    {
      sound_status |= 0x01;
      sample_data = gs->sample_data;
      envelope_volume = gs->envelope_volume;
      gbc_sound_render_channel(samples, 8, envelope, sweep);
    }

    gs = gbc_sound_channel + 1;
    if(gs->active_flag)
    {
      sound_status |= 0x02;
      sample_data = gs->sample_data;
      envelope_volume = gs->envelope_volume;
      gbc_sound_render_channel(samples, 8, envelope, nosweep);
    }

    gs = gbc_sound_channel + 2;
    if(gbc_sound_wave_update)
    {
      gbc_sound_load_wave_ram(gs->wave_bank);
      gbc_sound_wave_update = 0;
    }

    if((gs->active_flag) && (gs->master_enable))
    {
      sound_status |= 0x04;
      sample_data = wave_samples;

      if(gs->wave_type)
      {
        gbc_sound_render_channel(samples, 64, noenvelope, nosweep);
      }
      else
      {
        if(gs->wave_bank)
          sample_data += 32;

        gbc_sound_render_channel(samples, 32, noenvelope, nosweep);
      }
    }

    gs = gbc_sound_channel + 3;
    if(gs->active_flag)
    {
      sound_status |= 0x08;
      envelope_volume = gs->envelope_volume;

      if(gs->noise_type)
      {
        gbc_sound_render_channel(noise, half, envelope, nosweep);
      }
      else
      {
        gbc_sound_render_channel(noise, full, envelope, nosweep);
      }
    }
  }

  ADDRESS16(io_registers, 0x84) = sound_status;

  gbc_sound_last_cpu_ticks = cpu_ticks;
  gbc_sound_buffer_index =
   (gbc_sound_buffer_index + (buffer_ticks * 2)) % BUFFER_SIZE;
}


// Special thanks to blarrg for the LSFR frequency used in Meridian, as posted
// on the forum at http://meridian.overclocked.org:
// http://meridian.overclocked.org/cgi-bin/wwwthreads/showpost.pl?Board=merid
// angeneraldiscussion&Number=2069&page=0&view=expanded&mode=threaded&sb=4
// Hope you don't mind me borrowing it ^_-

static void init_noise_table(u32 *table, u32 period, u32 bit_length)
{
  u32 shift_register = 0xFF;
  u32 mask = ~(1 << bit_length);
  s32 table_pos, bit_pos;
  u32 current_entry;
  u32 table_period = (period + 31) / 32;

  // Bits are stored in reverse order so they can be more easily moved to
  // bit 31, for sign extended shift down.

  for(table_pos = 0; table_pos < table_period; table_pos++)
  {
    current_entry = 0;
    for(bit_pos = 31; bit_pos >= 0; bit_pos--)
    {
      current_entry |= (shift_register & 0x01) << bit_pos;

      shift_register =
       ((1 & (shift_register ^ (shift_register >> 1))) << bit_length) |
       ((shift_register >> 1) & mask);
    }

    table[table_pos] = current_entry;
  }
}


#define sound_copy_normal()                                                   \
  current_sample = source[i]                                                  \

#define sound_copy(source_offset, length, render_type)                        \
  _length = (length) >> 1;                                                    \
  source = (s16 *)(sound_buffer + source_offset);                             \
                                                                              \
  for(i = 0; i < _length; i++)                                                \
  {                                                                           \
    sound_copy_##render_type();                                               \
    if(current_sample > 2047)                                                 \
    {                                                                         \
      current_sample = 2047;                                                  \
    }                                                                         \
    if(current_sample < -2048)                                                \
    {                                                                         \
      current_sample = -2048;                                                 \
    }                                                                         \
    stream_base[i] = current_sample << 4;                                     \
    source[i] = 0;                                                            \
  }                                                                           \

#define sound_copy_null(source_offset, length)                                \
  _length = (length) >> 1;                                                    \
  source = (s16 *)(sound_buffer + source_offset);                             \
                                                                              \
  for(i = 0; i < _length; i++)                                                \
  {                                                                           \
    stream_base[i] = 0;                                                       \
    source[i] = 0;                                                            \
  }                                                                           \

static void sound_callback(s16 *stream, int length)
{
  u32 sample_length = length >> 1;
  u32 _length;
  u32 i;
  s16 *stream_base = stream;
  s16 *source;
  s32 current_sample;

  while(((gbc_sound_buffer_index - sound_buffer_base) % BUFFER_SIZE) < length)
  {
    sceKernelDelayThread(10);

    if(!sound_active)
      goto outloop;
  }

  if(global_enable_audio)
  {
    if((sound_buffer_base + sample_length) >= BUFFER_SIZE)
    {
      u32 partial_length = (BUFFER_SIZE - sound_buffer_base) << 1;
      sound_copy(sound_buffer_base, partial_length, normal);
      sound_copy(0, length - partial_length, normal);
      sound_buffer_base = (length - partial_length) >> 1;
    }
    else
    {
      sound_copy(sound_buffer_base, length, normal);
      sound_buffer_base += sample_length;
    }
  }
  else
  {
    if((sound_buffer_base + sample_length) >= BUFFER_SIZE)
    {
      u32 partial_length = (BUFFER_SIZE - sound_buffer_base) << 1;
      sound_copy_null(sound_buffer_base, partial_length);
      sound_copy_null(0, length - partial_length);
      sound_buffer_base = (length - partial_length) >> 1;
    }
    else
    {
      sound_copy_null(sound_buffer_base, length);
      sound_buffer_base += sample_length;
    }
  }

  outloop:;
}

static int sound_update_thread(SceSize args, void *argp)
{
  while(sound_active)
  {
    sound_callback(psp_sound_buffer, SOUND_BUFFER_SIZE);
    sceAudioOutputBlocking(sound_handle, sound_volume, (char *)psp_sound_buffer);
  }

  sceAudioChRelease(sound_handle);
  sound_handle = -1;

  sceKernelExitThread(0);

  return 0;
}

#define init_sound_thread()                                                   \
{                                                                             \
  sound_handle = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,                    \
                                   SOUND_BUFFER_SIZE / 4,                     \
                                   PSP_AUDIO_FORMAT_STEREO);                  \
  if(sound_handle < 0)                                                        \
  {                                                                           \
    quit();                                                                   \
  }                                                                           \
                                                                              \
  sound_thread = sceKernelCreateThread("Sound thread", sound_update_thread,   \
                                        0x11, 512, 0, NULL);                  \
  if(sound_thread < 0)                                                        \
  {                                                                           \
    sceAudioChRelease(sound_handle);                                          \
    sound_handle = -1;                                                        \
    quit();                                                                   \
  }                                                                           \
                                                                              \
  sound_active = 1;                                                           \
  sceKernelStartThread(sound_thread, 0, 0);                                   \
}                                                                             \

void init_sound(void)
{
  gbc_sound_tick_step = FLOAT_TO_FP16_16(256.0 / SOUND_FREQUENCY);

  init_noise_table(noise_table15, 32767, 14);
  init_noise_table(noise_table7, 127, 6);

  init_sound_thread();

  reset_sound();
}

void reset_sound(void)
{
  DIRECT_SOUND_STRUCT *ds = direct_sound_channel;
  GBC_SOUND_STRUCT *gs = gbc_sound_channel;
  u8 i;

  sound_on = 0;
  sound_buffer_base = 0;
  sound_last_cpu_ticks = 0;

  memset(sound_buffer, 0, BUFFER_SIZE);

  for(i = 0; i < 2; i++, ds++)
  {
    ds->buffer_index = 0;
    ds->status = DIRECT_SOUND_INACTIVE;
    ds->fifo_top = 0;
    ds->fifo_base = 0;
    ds->fifo_fractional = 0;
    ds->last_cpu_ticks = 0;
    memset(ds->fifo, 0, 32);
  }

  gbc_sound_buffer_index = 0;
  gbc_sound_last_cpu_ticks = 0;
  gbc_sound_partial_ticks = 0;

  gbc_sound_master_volume_left = 0;
  gbc_sound_master_volume_right = 0;
  gbc_sound_master_volume = 0;
  memset(wave_samples, 0, 64);

  for(i = 0; i < 4; i++, gs++)
  {
    gs->status = GBC_SOUND_INACTIVE;
    gs->sample_data = square_pattern_duty[2];
    gs->active_flag = 0;
  }

  sound_volume = PSP_AUDIO_VOLUME_MAX;
}

void sound_term(void)
{
  sound_active = 0;

  if(sound_thread >= 0)
  {
    sceKernelWaitThreadEnd(sound_thread, NULL);
    sceKernelDeleteThread(sound_thread);
    sound_thread = -1;
  }
}


#define sound_savestate_body(type)                                          \
{                                                                           \
  FILE_##type##_VARIABLE(savestate_file, sound_on);                         \
  FILE_##type##_VARIABLE(savestate_file, sound_buffer_base);                \
  FILE_##type##_VARIABLE(savestate_file, sound_last_cpu_ticks);             \
  FILE_##type##_VARIABLE(savestate_file, gbc_sound_buffer_index);           \
  FILE_##type##_VARIABLE(savestate_file, gbc_sound_last_cpu_ticks);         \
  FILE_##type##_VARIABLE(savestate_file, gbc_sound_partial_ticks);          \
  FILE_##type##_VARIABLE(savestate_file, gbc_sound_master_volume_left);     \
  FILE_##type##_VARIABLE(savestate_file, gbc_sound_master_volume_right);    \
  FILE_##type##_VARIABLE(savestate_file, gbc_sound_master_volume);          \
  FILE_##type##_ARRAY(savestate_file, wave_samples);                        \
  FILE_##type##_ARRAY(savestate_file, direct_sound_channel);                \
  FILE_##type##_ARRAY(savestate_file, gbc_sound_channel);                   \
}                                                                           \

void sound_read_savestate(FILE_TAG_TYPE savestate_file)
  sound_savestate_body(READ);

void sound_write_mem_savestate(FILE_TAG_TYPE savestate_file)
  sound_savestate_body(WRITE_MEM);

