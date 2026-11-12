#include "ili9341.h"
#include "main.h"

/* FSMC NE1, 16-bit bus. CPU address bit 17 drives FSMC_A16 (LCD RS). */
#define LCD_COMMAND (*(volatile uint16_t *)0x60000000UL)
#define LCD_DATA_ADDRESS 0x60020000UL
#define LCD_DATA    (*(volatile uint16_t *)LCD_DATA_ADDRESS)

#define LCD_DMA_FLAGS (DMA_LIFCR_CFEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CTEIF0 | \
                       DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTCIF0)
#define LCD_DMA_ERRORS (DMA_LISR_FEIF0 | DMA_LISR_DMEIF0 | DMA_LISR_TEIF0)
#define LCD_DMA_TIMEOUT_MS 20U
#define LCD_SCAN_TIMEOUT_MS 24U

/* Two PUPDR bits per FSMC data pin. Control pins keep their existing pulls. */
#define DATA_PULL_MASK_D 0xF03F000FUL /* PD0,1,8,9,10,14,15 */
#define DATA_PULL_MASK_E 0xFFFFC000UL /* PE7 through PE15 */

static uint8_t byte_shift;
static uint32_t last_fill_ms;
static uint32_t frame_start;
static uint32_t pixels_remaining;
static uint16_t dma_pixels;
static uint32_t scan_wait_ms;
static bool scan_synced;

#define SCREEN_PIXELS (ILI9341_WIDTH * ILI9341_HEIGHT)

enum
{
  LCD_SLEEP_OUT = 0x11,
  LCD_NORMAL_MODE_ON = 0x13,
  LCD_DISPLAY_ON = 0x29,
  LCD_COLUMN_ADDRESS = 0x2A,
  LCD_PAGE_ADDRESS = 0x2B,
  LCD_MEMORY_WRITE = 0x2C,
  LCD_MEMORY_READ = 0x2E,
  LCD_MEMORY_ACCESS_CONTROL = 0x36,
  LCD_PIXEL_FORMAT = 0x3A,
  LCD_GET_SCANLINE = 0x45,
  LCD_FRAME_RATE_CONTROL = 0xB1,
  LCD_INVERSION_CONTROL = 0xB4,
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
  probe->sync_wait_ms = scan_wait_ms;
  probe->scan_synced = scan_synced;
}

static void WriteCommand(uint8_t command)
{
  LCD_COMMAND = (uint16_t)command << byte_shift;
}

static void WriteParameter(uint8_t parameter)
{
  LCD_DATA = (uint16_t)parameter << byte_shift;
}

void ILI9341_SetFrameInversion(bool enabled)
{
  /* Keep the reset defaults for Idle and Partial modes. Toggle NLA only. */
  WriteCommand(LCD_INVERSION_CONTROL);
  WriteParameter(enabled ? 0x06 : 0x02);
}

static uint16_t ReadScanline(void)
{
  WriteCommand(LCD_GET_SCANLINE);
  (void)LCD_DATA; /* Dummy read */
  uint16_t high = (LCD_DATA >> byte_shift) & 0x03U;
  uint16_t low = (LCD_DATA >> byte_shift) & 0xFFU;
  return (high << 8) | low;
}

void ILI9341_WaitForScanStart(void)
{
  uint32_t start = HAL_GetTick();
  uint16_t previous = ReadScanline();
  scan_synced = false;
  do
  {
    uint16_t line = ReadScanline();
    /* Detect the wrap to the top, ignoring small backward read glitches. */
    if (previous >= ILI9341_HEIGHT / 2U && line < ILI9341_HEIGHT / 4U)
    {
      scan_synced = true;
      break;
    }
    previous = line;
  } while (HAL_GetTick() - start < LCD_SCAN_TIMEOUT_MS);
  scan_wait_ms = HAL_GetTick() - start;
}

uint32_t ILI9341_SetRefreshRate(bool low_rate)
{
  WriteCommand(LCD_FRAME_RATE_CONTROL);
  WriteParameter(0x00);
  WriteParameter(low_rate ? 0x1F : 0x1B);

  ILI9341_WaitForScanStart();
  if (!scan_synced)
  {
    return 0;
  }

  uint32_t start = HAL_GetTick();
  for (unsigned frame = 0; frame < 4U; ++frame)
  {
    ILI9341_WaitForScanStart();
    if (!scan_synced)
    {
      return 0;
    }
  }
  return HAL_GetTick() - start;
}

static void SetAddressRange(uint8_t command, uint16_t first, uint16_t last)
{
  WriteCommand(command);
  WriteParameter(first >> 8);
  WriteParameter(first & 0xFFU);
  WriteParameter(last >> 8);
  WriteParameter(last & 0xFFU);
}

static uint16_t PackRgb565(uint8_t red, uint8_t green, uint8_t blue)
{
  return ((uint16_t)(red & 0xF8U) << 8) |
         ((uint16_t)(green & 0xFCU) << 3) | (blue >> 3);
}

void ILI9341_ReadRow(uint16_t y, uint16_t pixels[ILI9341_WIDTH])
{
  SetAddressRange(LCD_COLUMN_ADDRESS, 0, ILI9341_WIDTH - 1U);
  SetAddressRange(LCD_PAGE_ADDRESS, y, y);
  WriteCommand(LCD_MEMORY_READ);
  (void)LCD_DATA;
  if (byte_shift == 0)
  {
    /* Each pair of pixels is returned as RG, BR, GB on the 16-bit bus. */
    for (unsigned x = 0; x < ILI9341_WIDTH; x += 2U)
    {
      uint16_t rg = LCD_DATA;
      uint16_t br = LCD_DATA;
      uint16_t gb = LCD_DATA;
      pixels[x] = PackRgb565(rg >> 8, rg, br >> 8);
      pixels[x + 1U] = PackRgb565(br, gb >> 8, gb);
    }
  }
  else
  {
    for (unsigned x = 0; x < ILI9341_WIDTH; ++x)
    {
      uint8_t red = LCD_DATA >> 8;
      uint8_t green = LCD_DATA >> 8;
      uint8_t blue = LCD_DATA >> 8;
      pixels[x] = PackRgb565(red, green, blue);
    }
  }
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
  WriteParameter(0x48); /* Portrait: reverse column order (MX), BGR panel order */
  WriteCommand(LCD_NORMAL_MODE_ON);
  WriteCommand(LCD_DISPLAY_ON);

  __HAL_RCC_DMA2_CLK_ENABLE();
  /* M2M routes the LCD destination through DMA2's AHB memory port (ES0182). */
  DMA2_Stream0->CR = DMA_SxCR_DIR_1 | DMA_SxCR_PINC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0;
  DMA2_Stream0->FCR = DMA_SxFCR_DMDIS | DMA_SxFCR_FTH_0;
}

void ILI9341_BeginFrame(void)
{
  frame_start = HAL_GetTick();
  pixels_remaining = SCREEN_PIXELS;
  SetAddressRange(LCD_COLUMN_ADDRESS, 0, ILI9341_WIDTH - 1U);
  SetAddressRange(LCD_PAGE_ADDRESS, 0, ILI9341_HEIGHT - 1U);
  WriteCommand(LCD_MEMORY_WRITE);
}

static void FinishPixels(uint32_t count)
{
  pixels_remaining -= count;
  if (pixels_remaining == 0)
  {
    __DSB();
    last_fill_ms = HAL_GetTick() - frame_start;
  }
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

  FinishPixels(count);
}

void ILI9341_StartPixels(const uint16_t *pixels, uint16_t count)
{
  if (count > pixels_remaining)
  {
    count = (uint16_t)pixels_remaining;
  }
  if (byte_shift != 0 || count == 0)
  {
    ILI9341_WritePixels(pixels, count);
    return;
  }

  DMA2->LIFCR = LCD_DMA_FLAGS;
  DMA2_Stream0->PAR = (uint32_t)(uintptr_t)pixels;
  DMA2_Stream0->M0AR = LCD_DATA_ADDRESS;
  DMA2_Stream0->NDTR = count;
  dma_pixels = count;
  __DSB();
  SET_BIT(DMA2_Stream0->CR, DMA_SxCR_EN);
}

bool ILI9341_WaitPixels(void)
{
  if (dma_pixels == 0)
  {
    return true;
  }

  uint32_t started = HAL_GetTick();
  uint32_t flags;
  do
  {
    flags = DMA2->LISR;
    if (HAL_GetTick() - started >= LCD_DMA_TIMEOUT_MS)
    {
      break;
    }
  } while (!(flags & (DMA_LISR_TCIF0 | LCD_DMA_ERRORS)));

  CLEAR_BIT(DMA2_Stream0->CR, DMA_SxCR_EN);
  while (DMA2_Stream0->CR & DMA_SxCR_EN)
  {
    if (HAL_GetTick() - started >= 2U * LCD_DMA_TIMEOUT_MS)
    {
      Error_Handler(); /* The active stream still owns its source buffer. */
    }
  }
  DMA2->LIFCR = LCD_DMA_FLAGS;

  uint16_t count = dma_pixels;
  dma_pixels = 0;
  if ((flags & (DMA_LISR_TCIF0 | LCD_DMA_ERRORS)) != DMA_LISR_TCIF0)
  {
    return false;
  }
  FinishPixels(count);
  return true;
}
