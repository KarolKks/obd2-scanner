#ifndef TASK_UART_H
#define TASK_UART_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "obd2.h"
#include "uart.h"

/**
 * @brief  Creates the FreeRTOS background UART telemetry stream task.
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure.
 */
BaseType_t Task_UART_Create(void);

/**
 * @brief  Retrieves the QueueHandle for UART telemetry updates.
 * @return QueueHandle_t of the UART telemetry queue (depth 1, overwrite).
 */
QueueHandle_t Task_UART_GetTelemetryQueue(void);

#endif /* TASK_UART_H */
