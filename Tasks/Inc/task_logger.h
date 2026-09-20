#ifndef TASK_LOGGER_H
#define TASK_LOGGER_H

#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "logger.h"

/**
 * @brief  Creates the FreeRTOS background SD logger task and its record queue.
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure.
 */
BaseType_t Task_Logger_Create(void);

/**
 * @brief  Retrieves the FreeRTOS QueueHandle for log records.
 * @return QueueHandle_t of the SD logging queue.
 */
QueueHandle_t Task_Logger_GetQueue(void);

/**
 * @brief  Enqueues a numeric record for SD logging.
 * @param  dt   Null-terminated timestamp string.
 * @param  name Null-terminated parameter name.
 * @param  val  Numeric value.
 * @param  unit Null-terminated unit string.
 * @return true if enqueued, false otherwise.
 */
bool Task_Logger_EnqueueNumeric(const char *dt, const char *name, uint32_t val, const char *unit);

/**
 * @brief  Enqueues a string record (e.g. VIN or DTC) for SD logging.
 * @param  dt      Null-terminated timestamp string.
 * @param  name    Null-terminated parameter name.
 * @param  str_val Null-terminated string value.
 * @return true if enqueued, false otherwise.
 */
bool Task_Logger_EnqueueString(const char *dt, const char *name, const char *str_val);

#endif /* TASK_LOGGER_H */
