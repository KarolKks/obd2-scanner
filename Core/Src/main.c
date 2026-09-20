#include "main.h"

/* SPI handle instance for SD card on SPI1 */
SPI_Handle_t hspi1 = {
    .instance = SPI1,
    .baudrate_div = LL_SPI_BAUDRATEPRESCALER_DIV256,
    .is_initialized = false
};

/**
 * @brief FreeRTOS Stack Overflow Hook (called when a task exceeds its stack watermark).
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    UART_SendString("\r\n[CRITICAL ERROR] FreeRTOS Stack Overflow in Task: ");
    UART_SendString(pcTaskName);
    UART_SendString("\r\n");
    while (1);
}

/**
 * @brief FreeRTOS Malloc Failed Hook (called when heap memory is exhausted).
 */
void vApplicationMallocFailedHook(void)
{
    UART_SendString("\r\n[CRITICAL ERROR] FreeRTOS Heap Exhausted (pvPortMalloc returned NULL)!\r\n");
    while (1);
}

int main(void)
{
    // Initialize core system clocks and 1ms SysTick timebase
    CLK_Init();

    // Initialize hardware RTC peripheral (with LSE/LSI fallback)
    RTC_Init();

    // Initialize UART (115200 baud, 8N1) for ST-Link Virtual COM Port
    UART_Init();
    UART_SendString("   OBD-II SCANNER (FreeRTOS Active)     \r\n");

    char current_time[24];
    if (RTC_GetDateTimeString(current_time, sizeof(current_time)) == RTC_OK) {
        UART_SendString("RTC Clock: ");
        UART_SendString(current_time);
        UART_SendString("\r\n");
    }

    // Initialize CAN hardware (500 kbps) and configure OBD-II response filter
    if (CAN_Init() != CAN_OK) {
        UART_SendString("[CAN ERROR] Initialization Failed!\r\n");
        while (1);
    }
    CAN_FilterOBD2();
    UART_SendString("CAN Bus Active (500 kbps, Filter 0x7E8-0x7EF).\r\n");

    // Initialize FatFs SD card filesystem and default logfile
    if (Logger_Init(&hspi1) != LOGGER_OK) {
        UART_SendString("[SD WARNING] Logging offline or card unmounted.\r\n");
    }

    // Initialize KY-040 Rotary Encoder (TIM3 PB4/PB5, Button PA10 / D2)
    if (KY040_Init() != KY040_OK) {
        UART_SendString("[ENC ERROR] KY-040 Initialization Failed!\r\n");
    } else {
        UART_SendString("KY-040 Encoder Ready (TIM3 PB4/PB5, Button PA10 / D2).\r\n");
    }

    // Create FreeRTOS Application Tasks with distinct priorities
    if (Task_OBD2_Create() != pdPASS) {
        UART_SendString("[RTOS ERROR] Failed to create Task_OBD2!\r\n");
        while (1);
    }

    if (Task_Logger_Create() != pdPASS) {
        UART_SendString("[RTOS ERROR] Failed to create Task_Logger!\r\n");
        while (1);
    }

    if (Task_UI_Create() != pdPASS) {
        UART_SendString("[RTOS ERROR] Failed to create Task_UI!\r\n");
        while (1);
    }

    UART_SendString("Starting FreeRTOS Scheduler...\r\n");

    // Start FreeRTOS real-time preemptive scheduler
    vTaskStartScheduler();

    // Code reaches here only if heap was insufficient to start scheduler
    UART_SendString("[FATAL] FreeRTOS Scheduler Terminated unexpectedly!\r\n");
    while (1);
}