#ifndef APP_LVGL_H
#define APP_LVGL_H

#include <stdbool.h>

void AppLvgl_Init(void);
void AppLvgl_Poll(bool touch_test_active);
void AppLvgl_StartVideo(void);

#endif
