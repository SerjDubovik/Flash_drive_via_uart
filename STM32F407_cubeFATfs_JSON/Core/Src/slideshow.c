#include "slideshow.h"

#include "fatfs.h"
#include "ili9341.h"
#include "main.h"

#include <stdbool.h>
#include <string.h>

#define IMAGE_DIRECTORY u"0:/image"
#define IMAGE_PREFIX IMAGE_DIRECTORY u"/"
#define IMAGE_BYTES (ILI9341_WIDTH * ILI9341_HEIGHT * sizeof(uint16_t))
#define IMAGE_HOLD_MS 3000U
#define IMAGE_RETRY_MS 1000U

/* The SDIO driver reads into 32-bit-aligned buffers. */
static uint16_t pixels[4096] __attribute__((aligned(4)));
static TCHAR current_name[_MAX_LFN + 1];
static uint32_t last_attempt_tick;
static uint32_t hold_ms;

static int CompareNames(const TCHAR *left, const TCHAR *right)
{
  while (*left && *left == *right)
  {
    ++left;
    ++right;
  }
  return (int)*left - (int)*right;
}

static bool IsImage(const FILINFO *entry)
{
  const TCHAR *extension = NULL;
  for (const TCHAR *name = entry->fname; *name; ++name)
  {
    if (*name == u'.')
    {
      extension = name;
    }
  }

  if ((entry->fattrib & AM_DIR) || entry->fsize != IMAGE_BYTES || !extension)
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

static bool SelectNextImage(void)
{
  DIR directory;
  FRESULT result = f_opendir(&directory, IMAGE_DIRECTORY);
  if (result == FR_NOT_ENABLED)
  {
    /* Share the SD volume registration used by the file browser. */
    result = f_mount(&SDFatFS, u"0:", 0);
    if (result == FR_OK)
    {
      result = f_opendir(&directory, IMAGE_DIRECTORY);
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
    if (!IsImage(&entry))
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

  memcpy(current_name, next[0] ? next : first, sizeof(current_name));
  return true;
}

static bool ShowCurrentImage(void)
{
  TCHAR path[_MAX_LFN + sizeof(IMAGE_PREFIX) / sizeof(TCHAR)] = IMAGE_PREFIX;
  memcpy(path + sizeof(IMAGE_PREFIX) / sizeof(TCHAR) - 1U,
         current_name, sizeof(current_name));

  FIL file;
  if (f_open(&file, path, FA_READ) != FR_OK)
  {
    return false;
  }

  UINT remaining = IMAGE_BYTES;
  while (remaining != 0U)
  {
    UINT requested = remaining < sizeof(pixels) ? remaining : sizeof(pixels);
    UINT received;
    if (f_read(&file, pixels, requested, &received) != FR_OK || received != requested)
    {
      break;
    }
    if (remaining == IMAGE_BYTES)
    {
      ILI9341_BeginFrame();
    }
    ILI9341_WritePixels(pixels, received / sizeof(pixels[0]));
    remaining -= received;
  }

  FRESULT result = f_close(&file);
  return remaining == 0U && result == FR_OK;
}

void Slideshow_Poll(void)
{
  if (HAL_GetTick() - last_attempt_tick < hold_ms)
  {
    return;
  }

  bool shown = SelectNextImage() && ShowCurrentImage();
  last_attempt_tick = HAL_GetTick();
  hold_ms = shown ? IMAGE_HOLD_MS : IMAGE_RETRY_MS;
}
