#ifndef TOUCHSCREEN_H
#define TOUCHSCREEN_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  bool pressed;
  uint16_t x;
  uint16_t y;
  uint16_t raw_x;
  uint16_t raw_y;
} TouchscreenSample;

bool Touchscreen_Read(TouchscreenSample *sample);
bool Touchscreen_ReadPosition(TouchscreenSample *sample);
bool Touchscreen_OwnsDisplay(void);

#endif
