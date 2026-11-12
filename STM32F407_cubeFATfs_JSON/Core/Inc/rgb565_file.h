#ifndef RGB565_FILE_H
#define RGB565_FILE_H

#include "fatfs.h"
#include "ili9341.h"
#include <stdbool.h>

#define RGB565_FRAME_BYTES (ILI9341_WIDTH * ILI9341_HEIGHT * sizeof(uint16_t))

bool Rgb565_OpenNext(FIL *file, const TCHAR *folder, TCHAR *current_name, bool video);
bool Rgb565_ReadFrame(FIL *file, bool sync_scan);
bool Rgb565_ReadStill(FIL *file);

#endif
