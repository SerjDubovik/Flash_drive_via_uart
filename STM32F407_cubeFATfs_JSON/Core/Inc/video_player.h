#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <stdbool.h>
#include <stdint.h>

/* True while video owns the display, including the interval between frames. */
bool VideoPlayer_Poll(void);
void VideoPlayer_SetPaused(bool paused);
bool VideoPlayer_ShowStill(void);
bool VideoPlayer_ReadStillRow(uint16_t y, uint16_t *pixels);

typedef struct
{
  uint32_t tick_ms;
  uint32_t frames;
  uint32_t late_frames;
  uint32_t frame_ms;
  unsigned target_fps;
} VideoPlayer_Stats;

VideoPlayer_Stats VideoPlayer_GetStats(void);
void VideoPlayer_Force30Fps(bool enabled);

#endif
