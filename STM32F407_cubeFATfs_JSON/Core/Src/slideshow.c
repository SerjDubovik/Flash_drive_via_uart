#include "slideshow.h"

#include "main.h"
#include "rgb565_file.h"

#define IMAGE_HOLD_MS 3000U
#define IMAGE_RETRY_MS 1000U

void Slideshow_Poll(void)
{
  static TCHAR current_name[_MAX_LFN + 1];
  static uint32_t last_attempt_tick;
  static uint32_t hold_ms;

  if (HAL_GetTick() - last_attempt_tick < hold_ms)
  {
    return;
  }

  FIL file;
  bool shown = false;
  if (Rgb565_OpenNext(&file, u"0:/image", current_name, false))
  {
    shown = Rgb565_ReadFrame(&file, false);
    shown = (f_close(&file) == FR_OK) && shown;
  }
  last_attempt_tick = HAL_GetTick();
  hold_ms = shown ? IMAGE_HOLD_MS : IMAGE_RETRY_MS;
}
