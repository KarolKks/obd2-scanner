#include "task_logger.h"

#define SD_QUEUE_CAPACITY       32U
#define LOGGER_TASK_STACK_SIZE  768U
#define LOGGER_TASK_PRIORITY    (tskIDLE_PRIORITY + 2)

static QueueHandle_t s_sd_queue = NULL;

static void Task_Logger_Body(void *argument)
{
    (void)argument;
    LogRecord_t rec;
    TickType_t last_sync_time = xTaskGetTickCount();

    while (1)
    {
        // Block waiting for new log records (up to 1000 ms timeout)
        if (xQueueReceive(s_sd_queue, &rec, pdMS_TO_TICKS(1000)) == pdTRUE) {
            Logger_WriteRecord(&rec);
        }

        // Periodic flush to physical Flash: every 2000 ms or when queue has been drained
        if (Logger_IsReady() && ((xTaskGetTickCount() - last_sync_time) >= pdMS_TO_TICKS(2000))) {
            Logger_Sync();
            last_sync_time = xTaskGetTickCount();
        }
    }
}

BaseType_t Task_Logger_Create(void)
{
    if (s_sd_queue == NULL) {
        s_sd_queue = xQueueCreate(SD_QUEUE_CAPACITY, sizeof(LogRecord_t));
        if (s_sd_queue == NULL) {
            return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
        }
    }

    return xTaskCreate(Task_Logger_Body, "Task_Logger", LOGGER_TASK_STACK_SIZE, NULL, LOGGER_TASK_PRIORITY, NULL);
}

QueueHandle_t Task_Logger_GetQueue(void)
{
    return s_sd_queue;
}

bool Task_Logger_EnqueueNumeric(const char *dt, const char *name, uint32_t val, const char *unit)
{
    if (s_sd_queue == NULL || dt == NULL || name == NULL || unit == NULL) {
        return false;
    }

    LogRecord_t rec;
    strncpy(rec.datetime, dt, sizeof(rec.datetime) - 1);
    rec.datetime[sizeof(rec.datetime) - 1] = '\0';

    strncpy(rec.param_name, name, sizeof(rec.param_name) - 1);
    rec.param_name[sizeof(rec.param_name) - 1] = '\0';

    rec.val = val;

    strncpy(rec.unit, unit, sizeof(rec.unit) - 1);
    rec.unit[sizeof(rec.unit) - 1] = '\0';

    rec.str_val[0] = '\0';
    rec.is_string = false;

    return (xQueueSend(s_sd_queue, &rec, 0) == pdPASS);
}

bool Task_Logger_EnqueueString(const char *dt, const char *name, const char *str_val)
{
    if (s_sd_queue == NULL || dt == NULL || name == NULL || str_val == NULL) {
        return false;
    }

    LogRecord_t rec;
    strncpy(rec.datetime, dt, sizeof(rec.datetime) - 1);
    rec.datetime[sizeof(rec.datetime) - 1] = '\0';

    strncpy(rec.param_name, name, sizeof(rec.param_name) - 1);
    rec.param_name[sizeof(rec.param_name) - 1] = '\0';

    rec.val = 0;
    rec.unit[0] = '\0';

    strncpy(rec.str_val, str_val, sizeof(rec.str_val) - 1);
    rec.str_val[sizeof(rec.str_val) - 1] = '\0';

    rec.is_string = true;

    return (xQueueSend(s_sd_queue, &rec, 0) == pdPASS);
}
