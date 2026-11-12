#include "rgb565_file.h"

#include <string.h>

#define FOLDER_PATH_MAX 16U

/* The SDIO driver reads into 32-bit-aligned buffers. */
static uint16_t pixels[2][4096] __attribute__((aligned(4)));

static int CompareNames(const TCHAR *left, const TCHAR *right)
{
  while (*left && *left == *right)
  {
    ++left;
    ++right;
  }
  return (int)*left - (int)*right;
}

static bool IsRgb565File(const FILINFO *entry, bool video)
{
  const TCHAR *extension = NULL;
  for (const TCHAR *name = entry->fname; *name; ++name)
  {
    if (*name == u'.')
    {
      extension = name;
    }
  }

  if ((entry->fattrib & AM_DIR) || !extension ||
      (video ? (entry->fsize <= RGB565_FRAME_BYTES || entry->fsize % RGB565_FRAME_BYTES != 0U)
             : (entry->fsize != RGB565_FRAME_BYTES)))
  {
    return false;
  }

  const TCHAR *expected = u".RGB565";
  while (*extension && ff_wtoupper(*extension) == *expected)
  {
    ++extension;
    ++expected;
  }
  return *extension == 0 && *expected == 0;
}

bool Rgb565_OpenNext(FIL *file, const TCHAR *folder, TCHAR *current_name, bool video)
{
  DIR directory;
  FRESULT result = f_opendir(&directory, folder);
  if (result == FR_NOT_ENABLED)
  {
    /* Share the SD volume registration used by the file browser. */
    result = f_mount(&SDFatFS, u"0:", 0);
    if (result == FR_OK)
    {
      result = f_opendir(&directory, folder);
    }
  }
  if (result != FR_OK)
  {
    return false;
  }

  FILINFO entry;
  TCHAR first[_MAX_LFN + 1] = {0};
  TCHAR next[_MAX_LFN + 1] = {0};
  while ((result = f_readdir(&directory, &entry)) == FR_OK && entry.fname[0])
  {
    if (!IsRgb565File(&entry, video))
    {
      continue;
    }
    if (!first[0] || CompareNames(entry.fname, first) < 0)
    {
      memcpy(first, entry.fname, sizeof(first));
    }
    if (CompareNames(entry.fname, current_name) > 0 &&
        (!next[0] || CompareNames(entry.fname, next) < 0))
    {
      memcpy(next, entry.fname, sizeof(next));
    }
  }
  f_closedir(&directory);
  if (result != FR_OK || !first[0])
  {
    return false;
  }

  memcpy(current_name, next[0] ? next : first, sizeof(first));
  TCHAR path[FOLDER_PATH_MAX + 1U + _MAX_LFN + 1U];
  size_t length = 0;
  while (length < FOLDER_PATH_MAX && folder[length])
  {
    path[length] = folder[length];
    ++length;
  }
  if (folder[length])
  {
    return false;
  }
  path[length++] = u'/';
  memcpy(path + length, current_name, sizeof(first));
  return f_open(file, path, FA_READ) == FR_OK;
}

static bool ReadFrame(FIL *file, bool use_dma, bool sync_scan)
{
  unsigned buffer = 0;
  UINT remaining = RGB565_FRAME_BYTES;
  while (remaining != 0U)
  {
    UINT requested = remaining < sizeof(pixels[0]) ? remaining : sizeof(pixels[0]);
    UINT received;
    FRESULT result = f_read(file, pixels[buffer], requested, &received);
    if (!ILI9341_WaitPixels() || result != FR_OK || received != requested)
    {
      return false;
    }
    if (remaining == RGB565_FRAME_BYTES)
    {
      if (sync_scan)
      {
        ILI9341_WaitForScanStart();
      }
      ILI9341_BeginFrame();
    }
    uint16_t count = received / sizeof(pixels[0][0]);
    if (use_dma)
    {
      ILI9341_StartPixels(pixels[buffer], count);
      buffer ^= 1U;
    }
    else
    {
      ILI9341_WritePixels(pixels[buffer], count);
    }
    remaining -= received;
  }

  return ILI9341_WaitPixels();
}

bool Rgb565_ReadFrame(FIL *file, bool sync_scan)
{
  return ReadFrame(file, true, sync_scan);
}

bool Rgb565_ReadStill(FIL *file)
{
  return ReadFrame(file, false, false);
}
