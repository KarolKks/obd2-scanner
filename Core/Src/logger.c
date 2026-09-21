#include "logger.h"
#include "fatfs_sd.h"

static FATFS s_fs;
static FIL s_logfile;
static bool s_sd_ready = false;

Logger_Status_t Logger_Init(SPI_Handle_t *hspi)
{
    if (hspi == NULL) {
        return LOGGER_ERR_INIT;
    }

    // Bind SPI bus instance to FatFs SD card driver
    SD_SPI_AttachBus(hspi);

    // Initialize SPI hardware interface if not already done
    if (!hspi->is_initialized) {
        if (SPI_Init(hspi) != SPI_STATUS_OK) {
            UART_SendString("[SPI ERROR] SPI Initialization Failed!\r\n");
            return LOGGER_ERR_INIT;
        }
        UART_SendString("SPI Hardware Ready.\r\n");
    }

    // Mount FatFs file system and open OBD2_LOG.CSV
    UART_SendString("Mounting SD Card...\r\n");
    FRESULT fr = f_mount(&s_fs, "", 1);
    if (fr == FR_OK) {
        fr = f_open(&s_logfile, "OBD2_LOG.CSV", FA_WRITE | FA_OPEN_APPEND);
        if (fr == FR_OK) {
            s_sd_ready = true;
            UART_SendString("SD Card Mounted. Log file 'OBD2_LOG.CSV' ready.\r\n");
            if (f_size(&s_logfile) == 0) {
                UINT bw = 0;
                const char *header = "DateTime,Parameter,Value,Unit\r\n";
                f_write(&s_logfile, header, (UINT)strlen(header), &bw);
                f_sync(&s_logfile);
            }
            return LOGGER_OK;
        } else {
            UART_SendString("Failed to open 'OBD2_LOG.CSV'! Error: ");
            UART_SendNumber((uint32_t)fr);
            UART_SendString("\r\n");
            return LOGGER_ERR_INIT;
        }
    } else {
        UART_SendString("SD Mount Failed! Error code: ");
        UART_SendNumber((uint32_t)fr);
        UART_SendString(" (check card insertion and wiring)\r\n");
        return LOGGER_ERR_INIT;
    }
}

Logger_Status_t Logger_WriteRecord(const LogRecord_t *rec)
{
    if (!s_sd_ready || rec == NULL) {
        return LOGGER_ERR_WRITE;
    }

    char line_buf[128];
    UINT bw = 0;

    if (rec->is_string) {
        snprintf(line_buf, sizeof(line_buf), "%s,%s,%s,\r\n",
                 rec->datetime, rec->param_name, rec->str_val);
    } else {
        snprintf(line_buf, sizeof(line_buf), "%s,%s,%lu,%s\r\n",
                 rec->datetime, rec->param_name, (unsigned long)rec->val, rec->unit);
    }

    FRESULT fr = f_write(&s_logfile, line_buf, (UINT)strlen(line_buf), &bw);
    return (fr == FR_OK) ? LOGGER_OK : LOGGER_ERR_WRITE;
}

void Logger_Sync(void)
{
    if (s_sd_ready) {
        f_sync(&s_logfile);
    }
}

bool Logger_IsReady(void)
{
    return s_sd_ready;
}
