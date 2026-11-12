#include "sd_browser.h"

#include "fatfs.h"
#include "ili9341.h"
#include "usbd_cdc_if.h"
#include "usart_ring.h"
#include "w25q16.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MAX_PATH_UNITS 512
/* Each UTF-16 unit can take six ASCII bytes in JSON. Allow two paths. */
#define MAX_FRAME (2 * MAX_PATH_UNITS * 6 + 128)
#define BLOCK_SIZE 2048
#define MAX_RESPONSE (BLOCK_SIZE * 2 + 128)
#define TRANSFER_IDLE_MS 30000U

enum { ID = 1, OP = 2, PATH = 4, TO = 8, SIZE = 16, OFFSET = 32, FIELD_CRC = 64, DATA = 128 };

typedef struct {
    uint32_t id;
    TCHAR op[8];
    TCHAR path[MAX_PATH_UNITS + 1];
    TCHAR to[MAX_PATH_UNITS + 1];
    unsigned fields;
    uint32_t size;
    uint32_t offset;
    uint32_t crc;
    UINT data_size;
} Request;

typedef enum { TRANSFER_NONE, TRANSFER_READ, TRANSFER_WRITE } TransferMode;
typedef enum { PORT_UART, PORT_USB, PORT_COUNT } BrowserPort;

typedef struct {
    char data[MAX_FRAME + 1];
    size_t length;
    bool receiving;
} BrowserInput;

typedef struct {
    FIL file;
    TransferMode mode;
    BrowserPort owner;
    uint32_t size;
    uint32_t last_activity;
    bool replace;
    TCHAR destination[MAX_PATH_UNITS + 1];
    TCHAR stage[16];
    TCHAR backup[16];
} Transfer;

static char response[MAX_RESPONSE];
static Request request;
static Transfer transfer;
static _Alignas(4) BYTE block[BLOCK_SIZE];
static const TCHAR sd_drive[] = u"0:";
static const TCHAR nor_drive[] = u"1:";
static const TCHAR *drive = sd_drive;
static BrowserInput inputs[PORT_COUNT];
static const TCHAR *volumes[PORT_COUNT] = {sd_drive, sd_drive};
static BrowserPort reply_port;
static uint32_t usb_session;

static void skip_space(const char **cursor)
{
    while (**cursor == ' ' || **cursor == '\t' || **cursor == '\r') {
        ++*cursor;
    }
}

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static bool read_string(const char **cursor, TCHAR *out, size_t capacity)
{
    const char *p = *cursor;
    size_t length = 0;
    if (*p++ != '"') return false;
    while (*p != '"') {
        unsigned ch = (unsigned char)*p++;
        if (ch < 0x20 || ch >= 0x7F || length + 1 >= capacity) return false;
        if (ch == '\\') {
            switch (*p++) {
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case 'u':
                    ch = 0;
                    for (unsigned i = 0; i < 4; ++i) {
                        int digit = hex_value(*p++);
                        if (digit < 0) return false;
                        ch = ch * 16 + (unsigned)digit;
                    }
                    if (!ch) return false;
                    break;
                default: return false;
            }
        }
        out[length++] = (TCHAR)ch;
    }
    out[length] = 0;
    *cursor = p + 1;
    return true;
}

static bool read_uint(const char **cursor, uint32_t *number)
{
    const char *p = *cursor;
    uint32_t value = 0;
    if (*p < '0' || *p > '9') return false;
    if (*p == '0' && p[1] >= '0' && p[1] <= '9') return false;
    do {
        unsigned digit = (unsigned)(*p++ - '0');
        if (value > (UINT32_MAX - digit) / 10) return false;
        value = value * 10 + digit;
    } while (*p >= '0' && *p <= '9');
    *number = value;
    *cursor = p;
    return true;
}

static bool read_data(const char **cursor, UINT *size)
{
    const char *p = *cursor;
    *size = 0;
    if (*p++ != '"') return false;
    while (*p != '"') {
        if (*size == BLOCK_SIZE) return false;
        int high = hex_value(*p++);
        if (high < 0) return false;
        int low = hex_value(*p++);
        if (low < 0) return false;
        block[(*size)++] = (BYTE)((high << 4) | low);
    }
    *cursor = p + 1;
    return true;
}

static bool equal(const TCHAR *left, const TCHAR *right)
{
    while (*left && *left == *right) {
        ++left;
        ++right;
    }
    return *left == *right;
}

static bool parse_request(const char *p)
{
    unsigned fields = 0;
    request.id = 0;
    request.op[0] = request.path[0] = request.to[0] = 0;

    skip_space(&p);
    if (*p++ != '{') return false;
    for (;;) {
        TCHAR key[7];
        unsigned field;
        skip_space(&p);
        if (!read_string(&p, key, sizeof(key) / sizeof(key[0]))) return false;
        skip_space(&p);
        if (*p++ != ':') return false;
        skip_space(&p);

        if (equal(key, u"id")) {
            field = ID;
            if ((fields & field) || !read_uint(&p, &request.id) || !request.id) return false;
        } else if (equal(key, u"op")) {
            field = OP;
            if (!read_string(&p, request.op, sizeof(request.op) / sizeof(request.op[0]))) return false;
        } else if (equal(key, u"path")) {
            field = PATH;
            if (!read_string(&p, request.path, MAX_PATH_UNITS + 1)) return false;
        } else if (equal(key, u"to")) {
            field = TO;
            if (!read_string(&p, request.to, MAX_PATH_UNITS + 1)) return false;
        } else if (equal(key, u"size")) {
            field = SIZE;
            if (!read_uint(&p, &request.size)) return false;
        } else if (equal(key, u"offset")) {
            field = OFFSET;
            if (!read_uint(&p, &request.offset)) return false;
        } else if (equal(key, u"crc")) {
            field = FIELD_CRC;
            if (!read_uint(&p, &request.crc)) return false;
        } else if (equal(key, u"data")) {
            field = DATA;
            if (!read_data(&p, &request.data_size)) return false;
        } else {
            return false;
        }

        if (fields & field) return false;
        fields |= field;
        skip_space(&p);
        if (*p == '}') break;
        if (*p++ != ',') return false;
    }
    ++p;
    skip_space(&p);
    request.fields = fields & ~(ID | OP);
    return !*p && (fields & (ID | OP)) == (ID | OP);
}

static bool valid_path(const TCHAR *path)
{
    if (*path++ != '/') return false;
    if (!*path) return true;
    for (;;) {
        const TCHAR *start = path;
        while (*path && *path != '/') {
            TCHAR ch = *path++;
            if (ch < 0x20 || ch == 0x7F || ch == '"' || ch == '*' || ch == ':' ||
                ch == '<' || ch == '>' || ch == '?' || ch == '\\' || ch == '|') {
                return false;
            }
            if (ch >= 0xD800 && ch <= 0xDBFF) {
                if (*path < 0xDC00 || *path > 0xDFFF) return false;
                ++path;
            } else if (ch >= 0xDC00 && ch <= 0xDFFF) {
                return false;
            }
        }
        if (path == start || path - start > _MAX_LFN ||
            path[-1] == '.' || path[-1] == ' ' || *start == ' ') return false;
        if (!*path) return true;
        ++path;
    }
}

static bool transmit(int length)
{
    if (length <= 0 || (size_t)length >= sizeof(response)) return false;
    if (reply_port == PORT_USB) {
        return CDC_Write_FS((const uint8_t *)response, (uint16_t)length, usb_session);
    }
    uint32_t timeout = 500U + (uint32_t)length * 10000U / MYUART.Init.BaudRate;
    return HAL_UART_Transmit(&MYUART, (uint8_t *)response, (uint16_t)length, timeout) == HAL_OK;
}

static void send_result(uint32_t id, FRESULT result)
{
    transmit(snprintf(response, sizeof(response),
        "{\"id\":%lu,\"result\":%u}\n", (unsigned long)id, (unsigned)result));
}

static bool send_entry(uint32_t id, const FILINFO *info)
{
    size_t length = (size_t)snprintf(response, sizeof(response),
        "{\"id\":%lu,\"name\":\"", (unsigned long)id);
    for (const TCHAR *p = info->fname; *p; ++p) {
        if (*p >= 0x20 && *p <= 0x7E && *p != '"' && *p != '\\') {
            response[length++] = (char)*p;
        } else {
            length += (size_t)snprintf(response + length, sizeof(response) - length,
                "\\u%04x", (unsigned)*p);
        }
    }
    int tail = snprintf(response + length, sizeof(response) - length,
        "\",\"directory\":%s,\"size\":%lu}\n",
        (info->fattrib & AM_DIR) ? "true" : "false", (unsigned long)info->fsize);
    return transmit((int)length + tail);
}

static FRESULT list_directory(const Request *req)
{
    DIR dir;
    FILINFO info;
    FRESULT result = f_opendir(&dir, req->path);
    if (result != FR_OK) return result;

    for (;;) {
        result = f_readdir(&dir, &info);
        if (result != FR_OK || !info.fname[0]) break;
        if (!send_entry(req->id, &info)) {
            result = FR_TIMEOUT;
            break;
        }
    }
    FRESULT closed = f_closedir(&dir);
    return result == FR_OK ? closed : result;
}

static FRESULT directory_move_allowed(const TCHAR *source, TCHAR *destination)
{
    DIR dir;
    FRESULT result = f_opendir(&dir, source);
    if (result != FR_OK) return result;
    DWORD source_cluster = dir.obj.sclust;
    result = f_closedir(&dir);
    if (result != FR_OK) return result;

    /* Compare directory identities, including paths that use an 8.3 alias. */
    TCHAR *end = destination;
    while (*end) ++end;
    while (--end > destination) {
        if (*end != '/') continue;
        *end = 0;
        result = f_opendir(&dir, destination);
        *end = '/';
        if (result != FR_OK) return result;
        DWORD ancestor_cluster = dir.obj.sclust;
        result = f_closedir(&dir);
        if (result != FR_OK) return result;
        if (source_cluster == ancestor_cluster) return FR_DENIED;
    }
    return FR_OK;
}

static FRESULT move_entry(Request *req)
{
    if (!valid_path(req->to) || !req->to[1]) return FR_INVALID_NAME;
    FILINFO info;
    FRESULT result = f_stat(req->path, &info);
    if (result != FR_OK) return result;
    if (info.fattrib & AM_DIR) {
        result = directory_move_allowed(req->path, req->to);
        if (result != FR_OK) return result;
    }
    return f_rename(req->path, req->to);
}

static uint32_t block_crc(UINT size)
{
    uint32_t crc = UINT32_MAX;
    for (UINT i = 0; i < size; ++i) {
        crc ^= block[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
        }
    }
    return ~crc;
}

static void transfer_name(TCHAR *path, uint32_t id, bool backup)
{
    char name[16];
    snprintf(name, sizeof(name), backup ? "/~B%08lX.BAK" : "/~U%08lX.TMP",
             (unsigned long)id);
    size_t i = 0;
    do {
        path[i] = (TCHAR)name[i];
    } while (name[i++]);
}

static FRESULT close_transfer(void)
{
    transfer.mode = TRANSFER_NONE;
    FRESULT result = f_close(&transfer.file);
    if (result != FR_OK) {
        /* FatFs may still hold the lock when f_close fails. */
        f_mount(NULL, drive, 0);
    }
    return result;
}

static FRESULT abort_transfer(void)
{
    if (transfer.mode == TRANSFER_NONE) return FR_OK;
    bool writing = transfer.mode == TRANSFER_WRITE;
    FRESULT result = close_transfer();
    return writing && result == FR_OK ? f_unlink(transfer.stage) : result;
}

static FRESULT begin_upload(const Request *req, bool replace)
{
    FILINFO info;
    FRESULT result = f_stat(req->path, &info);
    if (result == FR_OK) {
        if (!replace) return FR_EXIST;
        if (info.fattrib & (AM_DIR | AM_RDO)) return FR_DENIED;
    } else if (result != FR_NO_FILE || replace) {
        return result;
    }

    transfer_name(transfer.stage, req->id, false);
    transfer_name(transfer.backup, req->id, true);
    result = f_open(&transfer.file, transfer.stage, FA_CREATE_NEW | FA_WRITE);
    if (result != FR_OK) return result;
    memcpy(transfer.destination, req->path, sizeof(transfer.destination));
    transfer.mode = TRANSFER_WRITE;
    transfer.owner = reply_port;
    transfer.replace = replace;
    transfer.size = req->size;
    transfer.last_activity = HAL_GetTick();
    return FR_OK;
}

static FRESULT publish_upload(void)
{
    FRESULT result;
    if (transfer.replace) {
        result = f_rename(transfer.destination, transfer.backup);
        if (result != FR_OK) {
            f_unlink(transfer.stage);
            return result;
        }
    }

    result = f_rename(transfer.stage, transfer.destination);
    if (result != FR_OK) {
        if (transfer.replace && f_rename(transfer.backup, transfer.destination) != FR_OK) {
            /* Keep both recovery files if the original cannot be restored. */
            return FR_DISK_ERR;
        }
        f_unlink(transfer.stage);
        return result;
    }

    /* Once published, a cleanup failure must not undo the verified new file. */
    if (transfer.replace) f_unlink(transfer.backup);
    return FR_OK;
}

static FRESULT finish_transfer(void)
{
    if (transfer.mode == TRANSFER_NONE) return FR_INVALID_OBJECT;
    if (f_tell(&transfer.file) != transfer.size) return FR_INVALID_PARAMETER;
    bool writing = transfer.mode == TRANSFER_WRITE;
    FRESULT result = close_transfer();
    return writing && result == FR_OK ? publish_upload() : result;
}

static FRESULT write_block(const Request *req)
{
    if (transfer.mode != TRANSFER_WRITE) return FR_INVALID_OBJECT;
    if (req->offset != f_tell(&transfer.file) || !req->data_size ||
        req->data_size > transfer.size - req->offset ||
        req->crc != block_crc(req->data_size)) return FR_INVALID_PARAMETER;

    UINT written;
    FRESULT result = f_write(&transfer.file, block, req->data_size, &written);
    if (result != FR_OK || written != req->data_size) {
        FRESULT aborted = abort_transfer();
        if (aborted != FR_OK) return aborted;
        return result == FR_OK ? FR_DENIED : result;
    }
    transfer.last_activity = HAL_GetTick();
    return FR_OK;
}

static FRESULT read_block(const Request *req)
{
    if (transfer.mode != TRANSFER_READ) return FR_INVALID_OBJECT;
    if (req->offset != f_tell(&transfer.file) || req->offset >= transfer.size) {
        return FR_INVALID_PARAMETER;
    }

    UINT count = transfer.size - req->offset > BLOCK_SIZE ? BLOCK_SIZE : transfer.size - req->offset;
    UINT received;
    FRESULT result = f_read(&transfer.file, block, count, &received);
    if (result != FR_OK) return result;
    if (received != count) return FR_INT_ERR;

    static const char digits[] = "0123456789ABCDEF";
    size_t length = (size_t)snprintf(response, sizeof(response),
        "{\"id\":%lu,\"offset\":%lu,\"crc\":%lu,\"data\":\"",
        (unsigned long)req->id, (unsigned long)req->offset, (unsigned long)block_crc(received));
    for (UINT i = 0; i < received; ++i) {
        response[length++] = digits[block[i] >> 4];
        response[length++] = digits[block[i] & 15];
    }
    memcpy(response + length, "\"}\n", 3);
    transfer.last_activity = HAL_GetTick();
    return transmit((int)length + 3) ? FR_OK : FR_TIMEOUT;
}

static FRESULT begin_download(const Request *req)
{
    FRESULT result = f_open(&transfer.file, req->path, FA_READ);
    if (result != FR_OK) return result;
    transfer.mode = TRANSFER_READ;
    transfer.owner = reply_port;
    transfer.size = f_size(&transfer.file);
    transfer.last_activity = HAL_GetTick();
    return transmit(snprintf(response, sizeof(response),
        "{\"id\":%lu,\"size\":%lu}\n", (unsigned long)req->id,
        (unsigned long)transfer.size)) ? FR_OK : FR_TIMEOUT;
}

static FRESULT flash_diagnostics(uint32_t id)
{
    uint8_t jedec[3] = {0};
    uint8_t status[2] = {0};
    uint8_t sample[16];
    char hex[sizeof(sample) * 2 + 1] = {0};

    HAL_StatusTypeDef spi = W25Q16_ReadJedecId(jedec);
    bool detected = spi == HAL_OK && W25Q16_MatchesId(jedec);
    if (detected) {
        spi = W25Q16_ReadStatus(status);
        if (spi == HAL_OK && !(status[0] & W25Q16_STATUS_BUSY)) {
            spi = W25Q16_Read(0, sample, sizeof(sample));
            if (spi == HAL_OK) {
                for (size_t i = 0; i < sizeof(sample); ++i) {
                    snprintf(hex + i * 2, 3, "%02X", (unsigned)sample[i]);
                }
            }
        }
    }

    if (!transmit(snprintf(response, sizeof(response),
        "{\"id\":%lu,\"jedec\":\"%02X%02X%02X\",\"detected\":%s,"
        "\"sr1\":%u,\"sr2\":%u,\"data\":\"%s\",\"spi\":%u}\n",
        (unsigned long)id, (unsigned)jedec[0], (unsigned)jedec[1], (unsigned)jedec[2],
        detected ? "true" : "false", (unsigned)status[0], (unsigned)status[1], hex, (unsigned)spi))) {
        return FR_TIMEOUT;
    }
    if (spi == HAL_TIMEOUT) return FR_TIMEOUT;
    return detected && spi == HAL_OK ? FR_OK : FR_NOT_READY;
}

static FRESULT mount_volume(bool nor)
{
    FRESULT result = abort_transfer();
    if (result != FR_OK) return result;
    if (nor ? retNOR != 0 : retSD != 0) return FR_NOT_READY;

    const TCHAR *requested = nor ? nor_drive : sd_drive;
    result = f_mount(nor ? &NorFatFS : &SDFatFS, requested, 1);
    if (result == FR_OK) {
        result = f_chdrive(requested);
        if (result == FR_OK) drive = requested;
    }
    return result;
}

static FRESULT initialize_nor(void)
{
    static _Alignas(4) BYTE workspace[W25Q16_ERASE_SIZE];
    if (transfer.mode != TRANSFER_NONE) return FR_LOCKED;
    if (retNOR != 0) return FR_NOT_READY;

    FRESULT result = f_mount(&NorFatFS, nor_drive, 1);
    if (result == FR_OK) return FR_EXIST;
    if (result != FR_NO_FILESYSTEM) return result;

    return f_mkfs(nor_drive, FM_FAT | FM_SFD, W25Q16_ERASE_SIZE, workspace, sizeof(workspace));
}

static FRESULT report_free_space(uint32_t id)
{
    DWORD free_clusters;
    FATFS *volume;
    FRESULT result = f_getfree(drive, &free_clusters, &volume);
    if (result != FR_OK) return result;

    uint64_t bytes = (uint64_t)free_clusters * volume->csize;
#if _MAX_SS != _MIN_SS
    bytes *= volume->ssize;
#else
    bytes *= _MIN_SS;
#endif
    return transmit(snprintf(response, sizeof(response),
        "{\"id\":%lu,\"freeBytes\":%llu}\n", (unsigned long)id,
        (unsigned long long)bytes)) ? FR_OK : FR_TIMEOUT;
}

static FRESULT execute(Request *req)
{
    if (equal(req->op, u"lcdid") && !req->fields) {
        ILI9341_BusProbe probe;
        ILI9341_ProbeBus(&probe);
        return transmit(snprintf(response, sizeof(response),
            "{\"id\":%lu,\"d3\":[\"%04X\",\"%04X\",\"%04X\",\"%04X\"],"
            "\"d3_high\":[\"%04X\",\"%04X\",\"%04X\",\"%04X\"],"
            "\"reg0\":\"%04X\",\"pull_up\":\"%04X\",\"pull_down\":\"%04X\",\"fill_ms\":%lu}\n",
            (unsigned long)req->id, (unsigned)probe.d3[0], (unsigned)probe.d3[1],
            (unsigned)probe.d3[2], (unsigned)probe.d3[3],
            (unsigned)probe.d3_high[0], (unsigned)probe.d3_high[1],
            (unsigned)probe.d3_high[2], (unsigned)probe.d3_high[3],
            (unsigned)probe.reg0, (unsigned)probe.pull_up,
            (unsigned)probe.pull_down, (unsigned long)probe.fill_ms)) ? FR_OK : FR_TIMEOUT;
    }

    if (transfer.mode != TRANSFER_NONE && transfer.owner != reply_port) return FR_LOCKED;

    const TCHAR *volume = volumes[reply_port];
    if (drive != volume) {
        FRESULT result = f_chdrive(volume);
        if (result != FR_OK) return result;
        drive = volume;
    }
    if (equal(req->op, u"flashid") && !req->fields) return flash_diagnostics(req->id);
    if (equal(req->op, u"mount") && !req->fields) return mount_volume(false);
    if (equal(req->op, u"mntnor") && !req->fields) return mount_volume(true);
    if (equal(req->op, u"norinit") && !req->fields) return initialize_nor();
    if (equal(req->op, u"abort") && !req->fields) return abort_transfer();
    if (equal(req->op, u"finish") && !req->fields) return finish_transfer();
    if (equal(req->op, u"read") && req->fields == OFFSET) return read_block(req);
    if (equal(req->op, u"write") && req->fields == (OFFSET | FIELD_CRC | DATA)) return write_block(req);
    if (transfer.mode != TRANSFER_NONE) return FR_LOCKED;
    if (equal(req->op, u"space") && !req->fields) return report_free_space(req->id);
    if (!valid_path(req->path)) return FR_INVALID_NAME;
    if (equal(req->op, u"list")) return list_directory(req);
    if (!req->path[1]) return FR_INVALID_NAME;
    if (equal(req->op, u"get") && req->fields == PATH) return begin_download(req);
    bool replace = equal(req->op, u"replace");
    if ((replace || equal(req->op, u"put")) && req->fields == (PATH | SIZE)) {
        return begin_upload(req, replace);
    }
    if (equal(req->op, u"mkdir")) return f_mkdir(req->path);
    if (equal(req->op, u"delete")) return f_unlink(req->path);
    if (equal(req->op, u"move")) return move_entry(req);
    if (equal(req->op, u"create")) {
        FIL file;
        FRESULT result = f_open(&file, req->path, FA_CREATE_NEW | FA_WRITE);
        return result == FR_OK ? f_close(&file) : result;
    }
    return FR_INVALID_PARAMETER;
}

void SD_Browser_Init(void)
{
    __HAL_UART_ENABLE_IT(&MYUART, UART_IT_RXNE);
}

static void receive_byte(BrowserPort port, char ch)
{
    BrowserInput *input = &inputs[port];
    if (ch == 0x1E) {
        input->length = 0;
        input->receiving = true;
        return;
    }
    if (!input->receiving) return;

    reply_port = port;
    if (ch == '\n') {
        input->receiving = false;
        input->data[input->length] = 0;
        FRESULT result = parse_request(input->data) ? execute(&request) : FR_INVALID_PARAMETER;
        if (result == FR_OK && (equal(request.op, u"mount") || equal(request.op, u"mntnor"))) {
            volumes[port] = drive;
            transmit(snprintf(response, sizeof(response), "{\"id\":%lu,\"result\":0,\"version\":3}\n",
                (unsigned long)request.id));
        } else {
            send_result(request.id, result);
        }
    } else if (!ch || input->length == MAX_FRAME) {
        input->receiving = false;
        send_result(0, FR_INVALID_PARAMETER);
    } else {
        input->data[input->length++] = ch;
    }
}

void SD_Browser_Poll(void)
{
    uint32_t session = CDC_Session_FS();
    bool usb_reset = usb_session != session;
    if (usb_reset) {
        usb_session = session;
        inputs[PORT_USB].receiving = false;
        volumes[PORT_USB] = sd_drive;
    }
    if (transfer.mode != TRANSFER_NONE &&
        ((usb_reset && transfer.owner == PORT_USB) ||
         HAL_GetTick() - transfer.last_activity >= TRANSFER_IDLE_MS)) {
        abort_transfer();
    }

    uint8_t packet[CDC_DATA_FS_OUT_PACKET_SIZE];
    uint16_t length = CDC_ReadPacket_FS(packet, usb_session);
    for (uint16_t i = 0; i < length && CDC_Session_FS() == usb_session; ++i) {
        receive_byte(PORT_USB, (char)packet[i]);
    }

    uint16_t available = uart_available();
    while (available-- != 0U) {
        if (uart_discard_on_error()) {
            inputs[PORT_UART].receiving = false;
            reply_port = PORT_UART;
            send_result(0, FR_INVALID_PARAMETER);
            break;
        }
        receive_byte(PORT_UART, (char)uart_read());
    }
}
