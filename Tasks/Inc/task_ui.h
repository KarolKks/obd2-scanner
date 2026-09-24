#ifndef TASK_UI_H
#define TASK_UI_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "obd2.h"
#include "ky040.h"
#include "uart.h"
#include "sh1106.h"
#include "font.h"
#include "task_logger.h"

/**
 * @brief  Creates the FreeRTOS UI/Presentation task and its telemetry queue.
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure.
 */
BaseType_t Task_UI_Create(void);

/**
 * @brief  Retrieves the QueueHandle for vehicle telemetry updates.
 * @return QueueHandle_t of the telemetry queue (depth 1, for overwrite).
 */
QueueHandle_t Task_UI_GetTelemetryQueue(void);

#endif /* TASK_UI_H */
