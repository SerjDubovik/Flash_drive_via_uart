#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>

#define ILI9341_WIDTH  240U
#define ILI9341_HEIGHT 320U

void ILI9341_Init(void);
void ILI9341_BeginFrame(void);
void ILI9341_WritePixels(const uint16_t *pixels, uint32_t count);
typedef struct
{
  uint16_t d3[4];
  uint16_t d3_high[4];
  uint16_t pull_up;
  uint16_t pull_down;
  uint16_t reg0;
  uint32_t fill_ms;
} ILI9341_BusProbe;

void ILI9341_ProbeBus(ILI9341_BusProbe *probe);

#endif
