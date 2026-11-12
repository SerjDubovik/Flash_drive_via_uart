#include "video_player.h"

#include "main.h"
#include "rgb565_file.h"

#define VIDEO_DEFAULT_FPS 15U
#define VIDEO_FAST_FPS 30U
#define VIDEO_RETRY_MS 1000U

static bool playback_paused;
static FIL file;
static bool playing;
static bool force_30_fps;
static unsigned file_fps = VIDEO_DEFAULT_FPS;
static uint32_t next_tick;
static uint32_t frame_fraction;
static VideoPlayer_Stats stats;

static unsigned GetTargetFps(void)
{
  return force_30_fps ? VIDEO_FAST_FPS : file_fps;
}

static unsigned GetFileFps(const TCHAR *name)
{
  for (; *name; ++name)
  {
    const TCHAR *tag = u"_30FPS";
    const TCHAR *character = name;
    while (*tag && *character && ff_wtoupper(*character) == *tag)
    {
      ++tag;
      ++character;
    }
    if (!*tag && (*character == u'_' || *character == u'.' || !*character))
    {
      return VIDEO_FAST_FPS;
    }
  }
  return VIDEO_DEFAULT_FPS;
}

void VideoPlayer_Force30Fps(bool enabled)
{
  force_30_fps = enabled;
  next_tick = HAL_GetTick();
  frame_fraction = 0;
}

VideoPlayer_Stats VideoPlayer_GetStats(void)
{
  stats.tick_ms = HAL_GetTick();
  stats.target_fps = GetTargetFps();
  return stats;
}

void VideoPlayer_SetPaused(bool paused)
{
  if (playback_paused && !paused)
  {
    next_tick = HAL_GetTick();
    frame_fraction = 0;
  }
  playback_paused = paused;
}

bool VideoPlayer_ShowStill(void)
{
  if (!playing || f_tell(&file) < RGB565_FRAME_BYTES)
  {
    return false;
  }

  playback_paused = true;
  FSIZE_t position = f_tell(&file);
  bool shown = f_lseek(&file, position - RGB565_FRAME_BYTES) == FR_OK &&
               Rgb565_ReadStill(&file);
  if (!shown && f_lseek(&file, position) != FR_OK)
  {
    f_close(&file);
    playing = false;
  }
  return shown;
}

bool VideoPlayer_ReadStillRow(uint16_t y, uint16_t *pixels)
{
  if (!playback_paused || !playing || f_tell(&file) < RGB565_FRAME_BYTES ||
      y >= ILI9341_HEIGHT)
  {
    return false;
  }

  FSIZE_t position = f_tell(&file);
  const UINT row_bytes = ILI9341_WIDTH * sizeof(*pixels);
  UINT received;
  bool read = f_lseek(&file, position - RGB565_FRAME_BYTES + y * row_bytes) == FR_OK &&
              f_read(&file, pixels, row_bytes, &received) == FR_OK && received == row_bytes;
  if (f_lseek(&file, position) != FR_OK)
  {
    f_close(&file);
    playing = false;
    return false;
  }
  return read;
}

bool VideoPlayer_Poll(void)
{
  static TCHAR current_name[_MAX_LFN + 1];

  if (playback_paused)
  {
    return true;
  }

  uint32_t now = HAL_GetTick();
  if ((int32_t)(now - next_tick) < 0)
  {
    return playing;
  }

  if (playing && f_tell(&file) == f_size(&file))
  {
    f_close(&file);
    playing = false;
  }

  if (!playing)
  {
    playing = Rgb565_OpenNext(&file, u"0:/video", current_name, true);
    if (!playing)
    {
      next_tick = HAL_GetTick() + VIDEO_RETRY_MS;
      frame_fraction = 0;
      return false;
    }
    next_tick = HAL_GetTick();
    frame_fraction = 0;
    file_fps = GetFileFps(current_name);
  }

  uint32_t frame_start = HAL_GetTick();
  if (!Rgb565_ReadFrame(&file, true))
  {
    f_close(&file);
    playing = false;
    next_tick = HAL_GetTick() + VIDEO_RETRY_MS;
    frame_fraction = 0;
    return false;
  }

  unsigned fps = GetTargetFps();
  /* Carry the fractional milliseconds between frame deadlines. */
  frame_fraction += 1000U % fps;
  next_tick += 1000U / fps + frame_fraction / fps;
  frame_fraction %= fps;
  now = HAL_GetTick();
  stats.frame_ms = now - frame_start;
  ++stats.frames;
  if ((int32_t)(now - next_tick) > 0)
  {
    ++stats.late_frames;
    next_tick = now;
  }
  return true;
}
