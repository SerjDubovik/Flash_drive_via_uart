#ifndef W25Q16_H
#define W25Q16_H

#include "main.h"
#include <stdbool.h>

#define W25Q16_CAPACITY (2U * 1024U * 1024U)
#define W25Q16_STATUS_BUSY 0x01U
#define W25Q16_ERASE_SIZE 4096U
#define W25Q16_PAGE_SIZE 256U

HAL_StatusTypeDef W25Q16_ReadJedecId(uint8_t id[3]);
bool W25Q16_MatchesId(const uint8_t id[3]);
HAL_StatusTypeDef W25Q16_WaitReady(uint32_t timeout_ms);
HAL_StatusTypeDef W25Q16_ReadStatus(uint8_t status[2]);
HAL_StatusTypeDef W25Q16_Read(uint32_t address, uint8_t *data, uint16_t length);
HAL_StatusTypeDef W25Q16_EraseSector(uint32_t address);
HAL_StatusTypeDef W25Q16_ProgramPage(uint32_t address, const uint8_t *data, uint16_t length);

#endif
