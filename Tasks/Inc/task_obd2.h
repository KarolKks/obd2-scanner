#ifndef TASK_OBD2_H
#define TASK_OBD2_H

#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "obd2.h"
#include "task_logger.h"
#include "task_ui.h"
#include "rtc.h"
#include "clk.h"

/**
 * @brief  Creates the FreeRTOS high-priority OBD-II diagnostic acquisition task.
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure.
 */
BaseType_t Task_OBD2_Create(void);

#endif /* TASK_OBD2_H */
