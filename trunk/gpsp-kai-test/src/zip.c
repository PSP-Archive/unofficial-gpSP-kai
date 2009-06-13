/* unofficial gameplaySP kai
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 * Copyright (C) 2006 SiberianSTAR
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

#define ZIP_BUFFER_SIZE (320 * 1024) // 320KB

struct SZIPFileDataDescriptor
{
  s32 CRC32;
  s32 CompressedSize;
  s32 UncompressedSize;
} __attribute__((packed));

struct SZIPFileHeader
{
  char Sig[4]; // EDIT: Used to be s32 Sig;
  s16 VersionToExtract;
  s16 GeneralBitFlag;
  s16 CompressionMethod;
  s16 LastModFileTime;
  s16 LastModFileDate;
  struct SZIPFileDataDescriptor DataDescriptor;
  s16 FilenameLength;
  s16 ExtraFieldLength;
}  __attribute__((packed));


// ZIPで圧縮されたRONのロード
// 返り値:-2=一時ファイルを作成/-1=エラー/その他=ROMのサイズ
// もし、ROMのサイズ>ROMバッファのサイズ の場合は一時ファイルを作成
s32 load_file_zip(char *filename)
{
  struct SZIPFileHeader data;
  char tmp[MAX_PATH];
  s32 retval = -1;
  u8 *buffer = NULL;
  u8 *cbuffer;
  char *ext;
  FILE_ID fd;
  FILE_ID tmp_fd;
  u32 zip_buffer_size;
  u32 write_tmp_flag = NO;
  u32 use_vram;

  use_vram = NO;

  zip_buffer_size = 16 * 1024 * 1024;
  cbuffer = malloc(zip_buffer_size);

  while(cbuffer == NULL)
  {
    zip_buffer_size -= (256 * 1024);
    if(zip_buffer_size == 0) break;
    cbuffer = malloc(zip_buffer_size);
  }

  if(zip_buffer_size == 0)
  {
    use_vram = YES;
    zip_buffer_size = ZIP_BUFFER_SIZE;
    cbuffer = (u8 *)UNIVERSAL_VRAM_ADDR; // 汎用フレームバッファを使用
  }

  chdir(rom_path);
  FILE_OPEN(fd, filename, READ);

  if(!FILE_CHECK_VALID(fd))
    return -1;

  {
    FILE_READ(fd, &data, sizeof(struct SZIPFileHeader));

    // EDIT: Check if this is a zip file without worrying about endian
    // It checks for the following: 0x50 0x4B 0x03 0x04 (PK..)
    // Used to be: if(data.Sig != 0x04034b50) break;
	if( data.Sig[0] != 0x50 || data.Sig[1] != 0x4B ||
		data.Sig[2] != 0x03 || data.Sig[3] != 0x04 )
	{
		goto outcode;
	}

    FILE_READ(fd, tmp, data.FilenameLength);
    tmp[data.FilenameLength] = 0; // end string

    if(data.ExtraFieldLength)
      FILE_SEEK(fd, data.ExtraFieldLength, SEEK_CUR);

    if(data.GeneralBitFlag & 0x0008)
    {
      FILE_READ(fd, &data.DataDescriptor,
       sizeof(struct SZIPFileDataDescriptor));
    }

    ext = strrchr(tmp, '.') + 1;

    // file is too big
    if(data.DataDescriptor.UncompressedSize > gamepak_ram_buffer_size)
      {
        write_tmp_flag = YES; // テンポラリを使用するフラグをONに
        FILE_OPEN(tmp_fd, ZIP_TMP, WRITE);
      }
    else
      write_tmp_flag = NO;

    if(!strncasecmp(ext, "bin", 3) || !strncasecmp(ext, "gba", 3))
    {
      buffer = gamepak_rom;

      // ok, found
      switch(data.CompressionMethod)
      {
        case 0: //無圧縮
          retval = data.DataDescriptor.UncompressedSize;
          FILE_READ(fd, buffer, retval);

          goto outcode;

        case 8: //圧縮
        {
          z_stream stream = {0};
          s32 err;

          /*
           *   z.next_in = 入力ポインタ;
           *   z.avail_in = 入力データの残量;
           *   z.next_out = 出力ポインタ;
           *   z.avail_out = 出力バッファの残量;
           */

          stream.next_in = (Bytef*)cbuffer;
          stream.avail_in = (u32)zip_buffer_size;

          stream.next_out = (Bytef*)buffer;

          if(write_tmp_flag == NO)
            {
              stream.avail_out = data.DataDescriptor.UncompressedSize;
              retval = (u32)data.DataDescriptor.UncompressedSize;
            }
          else
            {
              stream.avail_out = gamepak_ram_buffer_size;
              retval = -2;
            }

          stream.zalloc = (alloc_func)0;
          stream.zfree = (free_func)0;
          stream.opaque = (voidpf)0;

          err = inflateInit2(&stream, -MAX_WBITS);

          FILE_READ(fd, cbuffer, zip_buffer_size);
          sceKernelDcacheWritebackAll();

          if(err == Z_OK)
          {
            while(err != Z_STREAM_END)
            {
              err = inflate(&stream, Z_SYNC_FLUSH);
              if(err == Z_BUF_ERROR)
              {
                stream.avail_in = (u32)zip_buffer_size;
                stream.next_in = (Bytef*)cbuffer;
                FILE_READ(fd, cbuffer, zip_buffer_size);
                sceKernelDcacheWritebackAll();
              }

              if((write_tmp_flag == YES) && (stream.avail_out == 0)) /* 出力バッファが尽きれば */
                {
                  FILE_WRITE(tmp_fd, buffer, gamepak_ram_buffer_size);
                  stream.next_out = buffer; /* 出力ポインタを元に戻す */
                  stream.avail_out = gamepak_ram_buffer_size; /* 出力バッファ残量を元に戻す */
                }
            }

            if((write_tmp_flag == YES) && ((gamepak_ram_buffer_size - stream.avail_out) != 0))
                FILE_WRITE(tmp_fd, buffer, gamepak_ram_buffer_size - stream.avail_out);

            err = Z_OK;
            inflateEnd(&stream);
          }
          goto outcode;
        }
      }
    }
  }

outcode:
  FILE_CLOSE(fd);

  if(write_tmp_flag == YES)
    FILE_CLOSE(tmp_fd);

  if(use_vram == NO)
    free(cbuffer);

  chdir(main_path);
  return retval;
}
