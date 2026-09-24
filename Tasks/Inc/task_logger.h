#ifndef TASK_LOGGER_H
#define TASK_LOGGER_H

#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "logger.h"
#include "spi.h"

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
 * @brief  Enqueues a complete vehicle telemetry snapshot for CSV logging.
 * @param  snap Pointer to snapshot data.
 * @return true if enqueued, false otherwise.
 */
bool Task_Logger_EnqueueSnapshot(const LogSnapshot_t *snap);

/**
 * @brief  Configures the logging interval in seconds (0 = disabled/paused).
 * @param  seconds Interval in seconds (0 to 120).
 */
void Task_Logger_SetIntervalSeconds(uint16_t seconds);

/**
 * @brief  Retrieves the current logging interval in seconds.
 * @return Interval in seconds (0 = disabled).
 */
uint16_t Task_Logger_GetIntervalSeconds(void);

#define LOGGER_ALL_CHANNELS_MASK 0x000FFFFFUL // All 20 channels enabled

/**
 * @brief  Configures the enabled channels bitmask for SD logging.
 * @param  mask Bitmask where bit i corresponds to PID descriptor index i.
 */
void Task_Logger_SetChannelsMask(uint32_t mask);

/**
 * @brief  Retrieves the current enabled channels bitmask.
 * @return 32-bit channels mask.
 */
uint32_t Task_Logger_GetChannelsMask(void);

/**
 * @brief  Checks whether a specific channel is enabled for logging.
 * @param  index Channel index (0 to 19).
 * @return true if enabled, false otherwise.
 */
bool Task_Logger_IsChannelEnabled(uint8_t index);

/**
 * @brief  Toggles the enabled state of a specific channel.
 * @param  index Channel index (0 to 19).
 */
void Task_Logger_ToggleChannel(uint8_t index);

/**
 * @brief  Selects or deselects all channels.
 * @param  select_all true to enable all 20 channels, false to disable all.
 */
void Task_Logger_SelectAllChannels(bool select_all);

/**
 * @brief  Returns the number of currently selected channels.
 * @return Number of set bits in mask (0 to 20).
 */
uint8_t Task_Logger_GetSelectedCount(void);

#endif /* TASK_LOGGER_H */
