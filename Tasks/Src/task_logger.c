#include "task_logger.h"

// FreeRTOS background SD logger task configuration
#define SD_QUEUE_CAPACITY       8U
#define LOGGER_TASK_STACK_SIZE  768U
#define LOGGER_TASK_PRIORITY    (tskIDLE_PRIORITY + 2)

// Queue for decoupling high-rate OBD-II polling from SPI SD card writes
static QueueHandle_t s_sd_queue = NULL;

// Runtime logging configuration (controlled via UI or defaults)
static volatile uint16_t s_log_interval_sec = 0;                         // 0 = OFF / paused, 1..60s = active
static volatile uint32_t s_log_channels_mask = LOGGER_ALL_CHANNELS_MASK; // Bitmask of enabled PID channels

static void Task_Logger_Body(void *argument)
{
    (void)argument;
    LogSnapshot_t snap;
    TickType_t last_reconnect_attempt = 0;

    while (1)
    {
        // Handle disconnected SD card: periodically attempt remount every 1s
        if (!Logger_IsReady()) {
            TickType_t now = xTaskGetTickCount();
            if ((now - last_reconnect_attempt) >= pdMS_TO_TICKS(1000)) {
                last_reconnect_attempt = now;
                // Acquire shared SPI bus mutex before card initialization
                if (SPI_Lock(200)) {
                    Logger_TryMount();
                    SPI_Unlock();
                }
            }
            // Discard stale incoming records while offline so queue does not overflow
            xQueueReceive(s_sd_queue, &snap, pdMS_TO_TICKS(100));
            continue;
        }

        // Wait up to 500ms for a new telemetry snapshot from OBD-II task
        if (xQueueReceive(s_sd_queue, &snap, pdMS_TO_TICKS(500)) == pdTRUE) {
            // Write formatted CSV row under shared SPI bus mutex
            if (SPI_Lock(200)) {
                Logger_WriteSnapshot(&snap);
                SPI_Unlock();
            }
        }
    }
}

BaseType_t Task_Logger_Create(void)
{
    // Create snapshot queue with buffer capacity for 8 snapshots
    if (s_sd_queue == NULL) {
        s_sd_queue = xQueueCreate(SD_QUEUE_CAPACITY, sizeof(LogSnapshot_t));
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

// Non-blocking snapshot submission from OBD-II core task
bool Task_Logger_EnqueueSnapshot(const LogSnapshot_t *snap)
{
    if (s_sd_queue == NULL || snap == NULL) {
        return false;
    }

    // Do not block if queue is full (drops record to preserve real-time OBD2 timing)
    return (xQueueSend(s_sd_queue, snap, 0) == pdPASS);
}

// Logging interval configuration (0 = paused)
void Task_Logger_SetIntervalSeconds(uint16_t seconds)
{
    s_log_interval_sec = seconds;
}

uint16_t Task_Logger_GetIntervalSeconds(void)
{
    return s_log_interval_sec;
}

// Channel bitmask configuration (bits 0..19 correspond to supported PIDs)
void Task_Logger_SetChannelsMask(uint32_t mask)
{
    s_log_channels_mask = mask & LOGGER_ALL_CHANNELS_MASK;
}

uint32_t Task_Logger_GetChannelsMask(void)
{
    return s_log_channels_mask;
}

bool Task_Logger_IsChannelEnabled(uint8_t index)
{
    if (index >= 20) return false;
    return (s_log_channels_mask & (1UL << index)) != 0;
}

// Toggle individual channel selection bit
void Task_Logger_ToggleChannel(uint8_t index)
{
    if (index < 20) {
        s_log_channels_mask ^= (1UL << index);
    }
}

// Enable or disable all 20 telemetry channels at once
void Task_Logger_SelectAllChannels(bool select_all)
{
    s_log_channels_mask = select_all ? LOGGER_ALL_CHANNELS_MASK : 0UL;
}

// Count number of currently selected channels (set bits)
uint8_t Task_Logger_GetSelectedCount(void)
{
    uint8_t count = 0;
    uint32_t m = s_log_channels_mask & LOGGER_ALL_CHANNELS_MASK;
    while (m) {
        count += (m & 1U);
        m >>= 1;
    }
    return count;
}
