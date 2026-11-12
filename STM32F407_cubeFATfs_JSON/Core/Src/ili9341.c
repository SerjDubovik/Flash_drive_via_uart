#include "ili9341.h"
#include "main.h"

/* FSMC NE1, 16-bit bus. CPU address bit 17 drives FSMC_A16 (LCD RS). */
#define LCD_COMMAND (*(volatile uint16_t *)0x60000000UL)
#define LCD_DATA    (*(volatile uint16_t *)0x60020000UL)

/* Two PUPDR bits per FSMC data pin. Control pins keep their existing pulls. */
#define DATA_PULL_MASK_D 0xF03F000FUL /* PD0,1,8,9,10,14,15 */
#define DATA_PULL_MASK_E 0xFFFFC000UL /* PE7 through PE15 */

static uint8_t byte_shift;
static uint32_t last_fill_ms;
static uint32_t frame_start;
static uint32_t pixels_remaining;

#define SCREEN_PIXELS (ILI9341_WIDTH * ILI9341_HEIGHT)

enum
{
  LCD_SLEEP_OUT = 0x11,
  LCD_NORMAL_MODE_ON = 0x13,
  LCD_DISPLAY_ON = 0x29,
  LCD_COLUMN_ADDRESS = 0x2A,
  LCD_PAGE_ADDRESS = 0x2B,
  LCD_MEMORY_WRITE = 0x2C,
  LCD_MEMORY_ACCESS_CONTROL = 0x36,
  LCD_PIXEL_FORMAT = 0x3A,
  LCD_READ_ID4 = 0xD3
};

static void ReadId4(uint16_t command, uint16_t id4[4])
{
  LCD_COMMAND = command;
  for (unsigned read = 0; read < 4; ++read)
  {
    id4[read] = LCD_DATA;
  }

}

void ILI9341_ProbeBus(ILI9341_BusProbe *probe)
{
  uint16_t samples[4];
  const uint32_t saved_d = GPIOD->PUPDR;
  const uint32_t saved_e = GPIOE->PUPDR;
  const uint16_t command = LCD_READ_ID4 << byte_shift;

  ReadId4(LCD_READ_ID4, probe->d3);
  ReadId4(LCD_READ_ID4 << 8, probe->d3_high);
  LCD_COMMAND = 0;
  probe->reg0 = LCD_DATA;

  GPIOD->PUPDR = (saved_d & ~DATA_PULL_MASK_D) | (DATA_PULL_MASK_D & 0x55555555UL);
  GPIOE->PUPDR = (saved_e & ~DATA_PULL_MASK_E) | (DATA_PULL_MASK_E & 0x55555555UL);
  ReadId4(command, samples);
  probe->pull_up = samples[3];

  GPIOD->PUPDR = (saved_d & ~DATA_PULL_MASK_D) | (DATA_PULL_MASK_D & 0xAAAAAAAAUL);
  GPIOE->PUPDR = (saved_e & ~DATA_PULL_MASK_E) | (DATA_PULL_MASK_E & 0xAAAAAAAAUL);
  ReadId4(command, samples);
  probe->pull_down = samples[3];

  GPIOD->PUPDR = saved_d;
  GPIOE->PUPDR = saved_e;
  probe->fill_ms = last_fill_ms;
}

static void WriteCommand(uint8_t command)
{
  LCD_COMMAND = (uint16_t)command << byte_shift;
}

static void WriteParameter(uint8_t parameter)
{
  LCD_DATA = (uint16_t)parameter << byte_shift;
}

static void SetAddressRange(uint8_t command, uint16_t first, uint16_t last)
{
  WriteCommand(command);
  WriteParameter(first >> 8);
  WriteParameter(first & 0xFFU);
  WriteParameter(last >> 8);
  WriteParameter(last & 0xFFU);
}

void ILI9341_Init(void)
{
  HAL_GPIO_WritePin(RST_Disp_GPIO_Port, RST_Disp_Pin, GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(RST_Disp_GPIO_Port, RST_Disp_Pin, GPIO_PIN_SET);
  HAL_Delay(120);

  uint16_t id4[4];
  byte_shift = 0;
  ReadId4(LCD_READ_ID4, id4);
  if ((id4[2] & 0xFFU) != 0x93U || (id4[3] & 0xFFU) != 0x41U)
  {
    ReadId4(LCD_READ_ID4 << 8, id4);
    if ((id4[2] >> 8) == 0x93U && (id4[3] >> 8) == 0x41U)
    {
      byte_shift = 8;
    }
  }

  WriteCommand(LCD_SLEEP_OUT);
  HAL_Delay(120);

  WriteCommand(LCD_PIXEL_FORMAT);
  WriteParameter(0x55); /* RGB565 */
  WriteCommand(LCD_MEMORY_ACCESS_CONTROL);
  WriteParameter(0x08); /* Portrait, BGR panel order */
  WriteCommand(LCD_NORMAL_MODE_ON);
  WriteCommand(LCD_DISPLAY_ON);
}

void ILI9341_BeginFrame(void)
{
  frame_start = HAL_GetTick();
  pixels_remaining = SCREEN_PIXELS;
  SetAddressRange(LCD_COLUMN_ADDRESS, 0, ILI9341_WIDTH - 1U);
  SetAddressRange(LCD_PAGE_ADDRESS, 0, ILI9341_HEIGHT - 1U);
  WriteCommand(LCD_MEMORY_WRITE);
}

void ILI9341_WritePixels(const uint16_t *pixels, uint32_t count)
{
  if (count > pixels_remaining)
  {
    count = pixels_remaining;
  }
  if (count == 0)
  {
    return;
  }

  if (byte_shift == 0)
  {
    for (uint32_t pixel = 0; pixel < count; ++pixel)
    {
      LCD_DATA = pixels[pixel];
    }
  }
  else
  {
    for (uint32_t pixel = 0; pixel < count; ++pixel)
    {
      uint16_t color = pixels[pixel];
      LCD_DATA = color & 0xFF00U;
      LCD_DATA = (uint16_t)(color << 8);
    }
  }

  pixels_remaining -= count;
  if (pixels_remaining == 0)
  {
    __DSB();
    last_fill_ms = HAL_GetTick() - frame_start;
  }
}
