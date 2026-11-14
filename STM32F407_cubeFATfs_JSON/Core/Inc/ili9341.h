#ifndef ILI9341_H
#define ILI9341_H

#include <stdbool.h>
#include <stdint.h>

#define ILI9341_WIDTH  240U
#define ILI9341_HEIGHT 320U

void ILI9341_Init(void);
void ILI9341_BeginFrame(void);
void ILI9341_BeginWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void ILI9341_WaitForScanStart(void);
void ILI9341_SetFrameInversion(bool enabled);
uint32_t ILI9341_SetRefreshRate(bool low_rate);
void ILI9341_WritePixels(const uint16_t *pixels, uint32_t count);
void ILI9341_ReadRow(uint16_t y, uint16_t pixels[ILI9341_WIDTH]);
/* Wait before reusing the source buffer or issuing another LCD command. */
void ILI9341_StartPixels(const uint16_t *pixels, uint16_t count);
bool ILI9341_WaitPixels(void);
typedef struct
{
  uint16_t d3[4];
  uint16_t d3_high[4];
  uint16_t pull_up;
  uint16_t pull_down;
  uint16_t reg0;
  uint32_t fill_ms;
  uint32_t sync_wait_ms;
  bool scan_synced;
} ILI9341_BusProbe;

void ILI9341_ProbeBus(ILI9341_BusProbe *probe);

#endif
