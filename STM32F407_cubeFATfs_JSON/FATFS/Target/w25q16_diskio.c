#include "w25q16_diskio.h"
#include "w25q16.h"

#include <string.h>

#define SECTOR_COUNT (W25Q16_CAPACITY / W25Q16_ERASE_SIZE)

static bool ready;
static uint8_t previous[W25Q16_ERASE_SIZE];

static DSTATUS nor_status(BYTE lun)
{
    return ready ? 0 : STA_NOINIT;
}

static DSTATUS nor_initialize(BYTE lun)
{
    uint8_t id[3];
    ready = W25Q16_WaitReady(1000) == HAL_OK &&
            W25Q16_ReadJedecId(id) == HAL_OK && W25Q16_MatchesId(id);
    return nor_status(lun);
}

static bool valid_range(DWORD sector, UINT count)
{
    return sector < SECTOR_COUNT && count && count <= SECTOR_COUNT - sector;
}

static DRESULT nor_read(BYTE lun, BYTE *data, DWORD sector, UINT count)
{
    if (!data || !valid_range(sector, count)) return RES_PARERR;
    if (!ready) return RES_NOTRDY;
    for (UINT i = 0; i < count; ++i) {
        if (W25Q16_Read((sector + i) * W25Q16_ERASE_SIZE, data, W25Q16_ERASE_SIZE) != HAL_OK) {
            ready = false;
            return RES_ERROR;
        }
        data += W25Q16_ERASE_SIZE;
    }
    return RES_OK;
}

static bool erased(const uint8_t *data, size_t length)
{
    while (length--) {
        if (*data++ != 0xFF) return false;
    }
    return true;
}

static bool write_sector(uint32_t address, const BYTE *data)
{
    if (W25Q16_Read(address, previous, sizeof(previous)) != HAL_OK) return false;
    if (memcmp(previous, data, sizeof(previous)) == 0) return true;
    if (!erased(previous, sizeof(previous)) && W25Q16_EraseSector(address) != HAL_OK) return false;

    for (uint32_t offset = 0; offset < W25Q16_ERASE_SIZE; offset += W25Q16_PAGE_SIZE) {
        if (erased(data + offset, W25Q16_PAGE_SIZE)) continue;
        if (W25Q16_ProgramPage(address + offset, data + offset, W25Q16_PAGE_SIZE) != HAL_OK) return false;
    }
    return W25Q16_Read(address, previous, sizeof(previous)) == HAL_OK &&
           memcmp(previous, data, sizeof(previous)) == 0;
}

static DRESULT nor_write(BYTE lun, const BYTE *data, DWORD sector, UINT count)
{
    if (!data || !valid_range(sector, count)) return RES_PARERR;
    if (!ready) return RES_NOTRDY;
    for (UINT i = 0; i < count; ++i) {
        if (!write_sector((sector + i) * W25Q16_ERASE_SIZE, data)) {
            ready = false;
            return RES_ERROR;
        }
        data += W25Q16_ERASE_SIZE;
    }
    return RES_OK;
}

static DRESULT nor_ioctl(BYTE lun, BYTE command, void *data)
{
    if (!ready) return RES_NOTRDY;
    switch (command) {
        case CTRL_SYNC:
            return W25Q16_WaitReady(1000) == HAL_OK ? RES_OK : RES_ERROR;
        case GET_SECTOR_COUNT:
            *(DWORD *)data = SECTOR_COUNT;
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD *)data = W25Q16_ERASE_SIZE;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD *)data = 1;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

const Diskio_drvTypeDef W25Q16_Driver = {
    nor_initialize, nor_status, nor_read, nor_write, nor_ioctl
};
