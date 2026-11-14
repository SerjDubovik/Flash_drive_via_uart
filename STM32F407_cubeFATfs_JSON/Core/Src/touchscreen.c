#include "touchscreen.h"

#include "ili9341.h"
#include "main.h"

#define TOUCH_DISPLAY_IDLE_MS 1000U
#define DOT_RADIUS 4
#define XPT2046_READ_X 0xD0U
#define XPT2046_READ_Y 0x90U

/* Starting calibration. Use the raw corner readings to tune the panel. */
#define TOUCH_RAW_X_LEFT 3900
#define TOUCH_RAW_X_RIGHT 200
#define TOUCH_RAW_Y_TOP 3900
#define TOUCH_RAW_Y_BOTTOM 200

extern SPI_HandleTypeDef hspi2;

static bool display_active;
static uint32_t last_request;
static TouchscreenSample previous;

static bool read_axis(uint8_t command, uint16_t *raw)
{
  uint8_t tx[3] = {command, 0, 0};
  uint8_t rx[3];
  HAL_GPIO_WritePin(T_CS_GPIO_Port, T_CS_Pin, GPIO_PIN_RESET);
  HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi2, tx, rx, sizeof(tx), 5U);
  HAL_GPIO_WritePin(T_CS_GPIO_Port, T_CS_Pin, GPIO_PIN_SET);
  if (status != HAL_OK)
  {
    return false;
  }
  *raw = ((((uint16_t)rx[1] << 8) | rx[2]) >> 3) & 0x0FFFU;
  return true;
}

static uint16_t scale_axis(uint16_t raw, int start, int end, int pixels)
{
  int coordinate = ((int)raw - start) * (pixels - 1) / (end - start);
  if (coordinate < 0) return 0;
  if (coordinate >= pixels) return (uint16_t)(pixels - 1);
  return (uint16_t)coordinate;
}

static void draw_dot(const TouchscreenSample *sample, uint16_t color)
{
  int x = sample->x;
  int y = sample->y;
  for (int dy = -DOT_RADIUS; dy <= DOT_RADIUS; ++dy)
  {
    if (y + dy < 0 || y + dy >= (int)ILI9341_HEIGHT) continue;
    int dx = DOT_RADIUS;
    while (dx * dx + dy * dy > DOT_RADIUS * DOT_RADIUS) --dx;
    int left = x - dx < 0 ? 0 : x - dx;
    int right = x + dx >= (int)ILI9341_WIDTH ? (int)ILI9341_WIDTH - 1 : x + dx;
    ILI9341_FillRect(left, y + dy, right - left + 1, 1, color);
  }
}

bool Touchscreen_ReadPosition(TouchscreenSample *sample)
{
  *sample = (TouchscreenSample){0};
  bool spi_ok = true;
  if (HAL_GPIO_ReadPin(T_IRQ_GPIO_Port, T_IRQ_Pin) == GPIO_PIN_RESET)
  {
    spi_ok = read_axis(XPT2046_READ_X, &sample->raw_x) &&
             read_axis(XPT2046_READ_Y, &sample->raw_y);
    sample->pressed = spi_ok && HAL_GPIO_ReadPin(T_IRQ_GPIO_Port, T_IRQ_Pin) == GPIO_PIN_RESET;
    if (sample->pressed)
    {
      sample->x = scale_axis(sample->raw_x, TOUCH_RAW_X_LEFT, TOUCH_RAW_X_RIGHT, ILI9341_WIDTH);
      sample->y = scale_axis(sample->raw_y, TOUCH_RAW_Y_TOP, TOUCH_RAW_Y_BOTTOM, ILI9341_HEIGHT);
    }
  }
  return spi_ok;
}

bool Touchscreen_Read(TouchscreenSample *sample)
{
  if (!Touchscreen_OwnsDisplay())
  {
    if (!ILI9341_WaitPixels()) return false;
    ILI9341_FillRect(0, 0, ILI9341_WIDTH, ILI9341_HEIGHT, 0);
    display_active = true;
  }
  last_request = HAL_GetTick();
  bool spi_ok = Touchscreen_ReadPosition(sample);
  if (previous.pressed != sample->pressed || previous.x != sample->x || previous.y != sample->y)
  {
    if (previous.pressed) draw_dot(&previous, 0);
    if (sample->pressed) draw_dot(sample, 0xF800U);
    previous = *sample;
  }
  return spi_ok;
}

bool Touchscreen_OwnsDisplay(void)
{
  if (display_active && HAL_GetTick() - last_request >= TOUCH_DISPLAY_IDLE_MS)
  {
    display_active = false;
    previous = (TouchscreenSample){0};
  }
  return display_active;
}
