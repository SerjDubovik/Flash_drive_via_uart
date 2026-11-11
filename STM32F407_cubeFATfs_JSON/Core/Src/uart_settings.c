#include "uart_settings.h"
#include "fatfs.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BAUD 115200U

static const char *skip_space(const char *text)
{
    return text + strspn(text, " \t\r\n");
}

static uint32_t parse_baud_rate(const char *text)
{
    if (strncmp(text, "\xEF\xBB\xBF", 3) == 0) text += 3;
    text = skip_space(text);
    if (*text != '{') return DEFAULT_BAUD;
    text = skip_space(text + 1);

    static const char key[] = "\"baudRate\"";
    if (strncmp(text, key, sizeof(key) - 1) != 0) return DEFAULT_BAUD;
    text = skip_space(text + sizeof(key) - 1);
    if (*text != ':') return DEFAULT_BAUD;
    text = skip_space(text + 1);
    if (*text < '1' || *text > '9') return DEFAULT_BAUD;

    char *end;
    unsigned long baud = strtoul(text, &end, 10);
    text = skip_space(end);
    if (*text != '}' || *skip_space(text + 1)) return DEFAULT_BAUD;

    switch (baud) {
        case 9600: case 19200: case 38400: case 57600:
        case 115200: case 230400: case 460800:
            return (uint32_t)baud;
        default:
            return DEFAULT_BAUD;
    }
}

uint32_t UART_Settings_LoadBaudRate(void)
{
    static const TCHAR drive[] = u"1:";
    if (retNOR != 0) return DEFAULT_BAUD;

    uint32_t baud = DEFAULT_BAUD;
    if (f_mount(&NorFatFS, drive, 1) == FR_OK) {
        FIL file;
        if (f_open(&file, u"1:/sys/settings_uart.json", FA_READ) == FR_OK) {
            char text[129];
            FSIZE_t length = f_size(&file);
            UINT received = 0;
            bool read_ok = length < sizeof(text) &&
                f_read(&file, text, (UINT)length, &received) == FR_OK &&
                received == length && !memchr(text, 0, received);
            if (f_close(&file) == FR_OK && read_ok) {
                text[received] = 0;
                baud = parse_baud_rate(text);
            }
        }
    }
    f_mount(NULL, drive, 0);
    return baud;
}
