#include "lcd_playback_diagnostics.h"

#include "main.h"
#include "rgb565_file.h"

#define PHASE_MS 8000U
#define UPDATE_MS 250U
#define CELL_SIZE 30U
#define BORDER_COLOR 0xFFFFU
#define LINE_COLOR 0x632CU
#define CELL_DARK 0x18E3U
#define CELL_LIGHT 0xA514U
#define MISSING_FILE_COLOR 0xF800U

enum { BORDER_ROW, LINE_ROW, EVEN_ROW, ODD_ROW };

static uint16_t rows[4][ILI9341_WIDTH];
static FIL fixture;
static bool fixture_open;

static void PrepareRows(void)
{
  for (unsigned x = 0; x < ILI9341_WIDTH; ++x)
  {
    bool edge = x == 0U || x == ILI9341_WIDTH - 1U;
    uint16_t even = (x / CELL_SIZE) % 2U ? CELL_LIGHT : CELL_DARK;
    uint16_t odd = (x / CELL_SIZE) % 2U ? CELL_DARK : CELL_LIGHT;
    rows[BORDER_ROW][x] = BORDER_COLOR;
    rows[LINE_ROW][x] = edge ? BORDER_COLOR : LINE_COLOR;
    if (x % CELL_SIZE == 0U)
    {
      even = LINE_COLOR;
      odd = LINE_COLOR;
    }
    rows[EVEN_ROW][x] = edge ? BORDER_COLOR : even;
    rows[ODD_ROW][x] = edge ? BORDER_COLOR : odd;
  }
}

static void DrawRamFrame(unsigned frame)
{
  ILI9341_BeginFrame();
  for (unsigned y = 0; y < ILI9341_HEIGHT; ++y)
  {
    unsigned row = EVEN_ROW + ((y / CELL_SIZE + frame) % 2U);
    if (y == 0U || y == ILI9341_HEIGHT - 1U)
    {
      row = BORDER_ROW;
    }
    else if (y % CELL_SIZE == 0U)
    {
      row = LINE_ROW;
    }
    ILI9341_WritePixels(rows[row], ILI9341_WIDTH);
  }
}

static bool OpenFixture(void)
{
  const TCHAR *path = u"0:/diag/grid.rgb565";
  FRESULT result = f_open(&fixture, path, FA_READ);
  if (result == FR_NOT_ENABLED && f_mount(&SDFatFS, u"0:", 0) == FR_OK)
  {
    result = f_open(&fixture, path, FA_READ);
  }
  if (result != FR_OK)
  {
    return false;
  }
  if (f_size(&fixture) != 2U * RGB565_FRAME_BYTES)
  {
    f_close(&fixture);
    return false;
  }
  return true;
}

static void ShowFixtureError(void)
{
  uint16_t row[ILI9341_WIDTH];
  for (unsigned x = 0; x < ILI9341_WIDTH; ++x)
  {
    row[x] = MISSING_FILE_COLOR;
  }
  ILI9341_BeginFrame();
  for (unsigned y = 0; y < ILI9341_HEIGHT; ++y)
  {
    ILI9341_WritePixels(row, ILI9341_WIDTH);
  }
}

void LcdPlaybackDiagnostics_Poll(void)
{
  static bool started;
  static bool use_sd;
  static uint32_t phase_start;
  static uint32_t last_update;
  static unsigned frame;
  uint32_t now = HAL_GetTick();

  if (!started)
  {
    PrepareRows();
    phase_start = now;
    last_update = now - UPDATE_MS;
    started = true;
  }

  if (now - phase_start >= PHASE_MS)
  {
    phase_start = now;
    use_sd = !use_sd;
    frame = 0;
    last_update = now - UPDATE_MS;
    HAL_GPIO_WritePin(UserLED_GPIO_Port, UserLED_Pin, use_sd ? GPIO_PIN_SET : GPIO_PIN_RESET);
    if (use_sd)
    {
      fixture_open = OpenFixture();
      if (!fixture_open)
      {
        ShowFixtureError();
      }
    }
    else if (fixture_open)
    {
      f_close(&fixture);
      fixture_open = false;
    }
  }

  if (now - last_update < UPDATE_MS)
  {
    return;
  }
  last_update = now;

  if (!use_sd)
  {
    DrawRamFrame(frame);
  }
  else if (fixture_open &&
           (f_lseek(&fixture, frame * RGB565_FRAME_BYTES) != FR_OK || !Rgb565_ReadFrame(&fixture, false)))
  {
    f_close(&fixture);
    fixture_open = false;
    ShowFixtureError();
  }
  frame ^= 1U;
}
