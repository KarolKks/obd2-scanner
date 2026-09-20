#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "spi.h"
#include "uart.h"

typedef enum {
    LOGGER_OK        = 0,
    LOGGER_ERR_INIT  = 1,
    LOGGER_ERR_WRITE = 2
} Logger_Status_t;

typedef struct {
    char     datetime[24];
    char     param_name[16];
    uint32_t val;
    char     unit[8];
    char     str_val[24];
    bool     is_string;
} LogRecord_t;

/**
 * @brief  Initializes SD card filesystem and opens the default CSV logfile.
 * @param  hspi Pointer to SPI handle configured for SD card.
 * @return LOGGER_OK if mounted and file opened, LOGGER_ERR_INIT otherwise.
 */
Logger_Status_t Logger_Init(SPI_Handle_t *hspi);

/**
 * @brief  Writes a single LogRecord_t to the open logfile.
 * @param  rec Pointer to the log record to serialize and write.
 * @return LOGGER_OK on success, LOGGER_ERR_WRITE on failure.
 */
Logger_Status_t Logger_WriteRecord(const LogRecord_t *rec);

/**
 * @brief  Flushes cached file buffers to physical SD flash memory.
 * @return None.
 */
void Logger_Sync(void);

/**
 * @brief  Checks whether the SD card logger is mounted and ready for writes.
 * @return true if ready, false otherwise.
 */
bool Logger_IsReady(void);

#endif /* LOGGER_H */
