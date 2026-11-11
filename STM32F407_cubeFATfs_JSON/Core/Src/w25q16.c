#include "w25q16.h"

#include <string.h>

#define SPI_TIMEOUT_MS 100U

extern SPI_HandleTypeDef hspi1;

static HAL_StatusTypeDef read_command(uint8_t *command, uint16_t command_length,
                                    uint8_t *data, uint16_t length)
{
    HAL_GPIO_WritePin(W25Q16_CS_GPIO_Port, W25Q16_CS_Pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef result = HAL_SPI_Transmit(&hspi1, command, command_length, SPI_TIMEOUT_MS);
    if (result == HAL_OK) {
        /* Full-duplex master receive clocks out the supplied buffer as dummy bytes. */
        memset(data, 0, length);
        result = HAL_SPI_Receive(&hspi1, data, length, SPI_TIMEOUT_MS);
    }
    HAL_GPIO_WritePin(W25Q16_CS_GPIO_Port, W25Q16_CS_Pin, GPIO_PIN_SET);
    return result;
}

HAL_StatusTypeDef W25Q16_ReadJedecId(uint8_t id[3])
{
    uint8_t command = 0x9F;
    return read_command(&command, 1, id, 3);
}

bool W25Q16_MatchesId(const uint8_t id[3])
{
    return id[0] == 0xEF && id[2] == 0x15 &&
           (id[1] == 0x40 || id[1] == 0x60 || id[1] == 0x70);
}

static HAL_StatusTypeDef read_status1(uint8_t *status)
{
    uint8_t command = 0x05;
    return read_command(&command, 1, status, 1);
}

HAL_StatusTypeDef W25Q16_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    do {
        uint8_t status;
        HAL_StatusTypeDef result = read_status1(&status);
        if (result != HAL_OK) return result;
        if (!(status & W25Q16_STATUS_BUSY)) return HAL_OK;
        HAL_Delay(1);
    } while (HAL_GetTick() - start < timeout_ms);
    return HAL_TIMEOUT;
}

HAL_StatusTypeDef W25Q16_ReadStatus(uint8_t status[2])
{
    HAL_StatusTypeDef result = read_status1(&status[0]);
    if (result != HAL_OK) return result;

    uint8_t command = 0x35;
    return read_command(&command, 1, &status[1], 1);
}

HAL_StatusTypeDef W25Q16_Read(uint32_t address, uint8_t *data, uint16_t length)
{
    if (!data || !length || address >= W25Q16_CAPACITY || length > W25Q16_CAPACITY - address) {
        return HAL_ERROR;
    }

    uint8_t command[] = {0x03, (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address};
    return read_command(command, sizeof(command), data, length);
}

static HAL_StatusTypeDef write_command(const uint8_t *command, uint16_t command_length,
                                      const uint8_t *data, uint16_t length)
{
    HAL_GPIO_WritePin(W25Q16_CS_GPIO_Port, W25Q16_CS_Pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef result = HAL_SPI_Transmit(&hspi1, command, command_length, SPI_TIMEOUT_MS);
    if (result == HAL_OK && length) {
        result = HAL_SPI_Transmit(&hspi1, data, length, SPI_TIMEOUT_MS);
    }
    HAL_GPIO_WritePin(W25Q16_CS_GPIO_Port, W25Q16_CS_Pin, GPIO_PIN_SET);
    return result;
}

static HAL_StatusTypeDef write_enable(void)
{
    const uint8_t command = 0x06;
    HAL_StatusTypeDef result = write_command(&command, 1, NULL, 0);
    if (result != HAL_OK) return result;

    uint8_t status;
    result = read_status1(&status);
    if (result != HAL_OK) return result;
    return (status & 0x03U) == 0x02U ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef W25Q16_EraseSector(uint32_t address)
{
    if (address >= W25Q16_CAPACITY || address % W25Q16_ERASE_SIZE) return HAL_ERROR;
    HAL_StatusTypeDef result = write_enable();
    if (result != HAL_OK) return result;

    const uint8_t command[] = {0x20, (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address};
    result = write_command(command, sizeof(command), NULL, 0);
    return result == HAL_OK ? W25Q16_WaitReady(1000) : result;
}

HAL_StatusTypeDef W25Q16_ProgramPage(uint32_t address, const uint8_t *data, uint16_t length)
{
    if (!data || !length || address >= W25Q16_CAPACITY ||
        length > W25Q16_PAGE_SIZE - address % W25Q16_PAGE_SIZE) {
        return HAL_ERROR;
    }
    HAL_StatusTypeDef result = write_enable();
    if (result != HAL_OK) return result;

    const uint8_t command[] = {0x02, (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address};
    result = write_command(command, sizeof(command), data, length);
    return result == HAL_OK ? W25Q16_WaitReady(50) : result;
}
