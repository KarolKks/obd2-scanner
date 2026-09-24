#include "spi.h"

#define SPI_TIMEOUT_LOOPS 100000U

SPI_Status_t SPI_Init(SPI_Handle_t *hspi)
{
    // Validate handle pointer and peripheral instance
    if (hspi == NULL || hspi->instance != SPI1) {
        return SPI_STATUS_ERROR;
    }

    // Enable SPI1 and GPIOA peripheral clocks
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    // Configure GPIOA pins for SPI1 (PA5: SCK, PA6: MISO, PA7: MOSI)
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_5, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_6, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_7, LL_GPIO_MODE_ALTERNATE);
    
    // Assign Alternate Function AF5 to all SPI1 signals
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_5, LL_GPIO_AF_5);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_6, LL_GPIO_AF_5);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_7, LL_GPIO_AF_5);

    // Set high-speed slew rate
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_5, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_6, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_7, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    
    // Explicitly configure Push-Pull output on SCK and MOSI lines
    LL_GPIO_SetPinOutputType(GPIOA, LL_GPIO_PIN_5, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinOutputType(GPIOA, LL_GPIO_PIN_7, LL_GPIO_OUTPUT_PUSHPULL);

    // Enable internal pull-ups on MISO and MOSI to prevent floating lines
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_5, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_6, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_7, LL_GPIO_PULL_UP);

    // Configure SPI1 master mode, 8-bit data width, MSB first
    LL_SPI_SetMode(hspi->instance, LL_SPI_MODE_MASTER);
    LL_SPI_SetStandard(hspi->instance, LL_SPI_PROTOCOL_MOTOROLA);
    LL_SPI_SetDataWidth(hspi->instance, LL_SPI_DATAWIDTH_8BIT);
    LL_SPI_SetTransferBitOrder(hspi->instance, LL_SPI_MSB_FIRST);
    
    // Set SPI Mode 0 (CPOL=0, CPHA=0)
    LL_SPI_SetClockPolarity(hspi->instance, LL_SPI_POLARITY_LOW);
    LL_SPI_SetClockPhase(hspi->instance, LL_SPI_PHASE_1EDGE);
    
    // Use software slave select (NSS managed manually via GPIO)
    LL_SPI_SetNSSMode(hspi->instance, LL_SPI_NSS_SOFT);
    
    // Set initial baud rate prescaler
    LL_SPI_SetBaudRatePrescaler(hspi->instance, hspi->baudrate_div);
    
    // Configure RX FIFO threshold to 8 bits for 1-byte granularity
    LL_SPI_SetRxFIFOThreshold(hspi->instance, LL_SPI_RX_FIFO_TH_QUARTER);

    // Enable SPI peripheral
    LL_SPI_Enable(hspi->instance);

    // Flush any residual data in RX FIFO
    while (LL_SPI_IsActiveFlag_RXNE(hspi->instance)) {
        (void)LL_SPI_ReceiveData8(hspi->instance);
    }

    hspi->is_initialized = true;

    return SPI_STATUS_OK;
}

SPI_Status_t SPI_SetBaudrate(SPI_Handle_t *hspi, uint32_t baudrate_div)
{
    if (hspi == NULL || !hspi->is_initialized) {
        return SPI_STATUS_ERROR;
    }

    uint32_t timeout = SPI_TIMEOUT_LOOPS;

    // Wait until SPI bus activity is idle before reconfiguring
    while (LL_SPI_IsActiveFlag_BSY(hspi->instance)) {
        if (--timeout == 0) return SPI_STATUS_TIMEOUT;
    }

    // Disable SPI peripheral to modify prescaler register safely
    LL_SPI_Disable(hspi->instance);
    
    // Apply new baud rate divider
    LL_SPI_SetBaudRatePrescaler(hspi->instance, baudrate_div);
    hspi->baudrate_div = baudrate_div;
    
    // Re-enable SPI peripheral
    LL_SPI_Enable(hspi->instance);

    // Flush RX FIFO
    while (LL_SPI_IsActiveFlag_RXNE(hspi->instance)) {
        (void)LL_SPI_ReceiveData8(hspi->instance);
    }

    return SPI_STATUS_OK;
}

SPI_Status_t SPI_TransferByte(SPI_Handle_t *hspi, uint8_t tx_byte, uint8_t *rx_byte)
{
    if (hspi == NULL || !hspi->is_initialized) {
        return SPI_STATUS_ERROR;
    }

    uint32_t timeout = SPI_TIMEOUT_LOOPS;

    // Wait until TX FIFO has room for a byte
    while (!LL_SPI_IsActiveFlag_TXE(hspi->instance)) {
        if (--timeout == 0) return SPI_STATUS_TIMEOUT;
    }
    
    // Send byte to SPI TX register
    LL_SPI_TransmitData8(hspi->instance, tx_byte);

    timeout = SPI_TIMEOUT_LOOPS;

    // Wait until received byte arrives in RX FIFO
    while (!LL_SPI_IsActiveFlag_RXNE(hspi->instance)) {
        if (--timeout == 0) return SPI_STATUS_TIMEOUT;
    }
    
    // Read received data byte
    uint8_t received_data = LL_SPI_ReceiveData8(hspi->instance);

    // Clear Overrun error flag if it occurred
    if (LL_SPI_IsActiveFlag_OVR(hspi->instance)) {
        LL_SPI_ClearFlag_OVR(hspi->instance);
        return SPI_STATUS_ERROR;
    }

    // Pass received byte back if output buffer is provided
    if (rx_byte != NULL) {
        *rx_byte = received_data;
    }

    return SPI_STATUS_OK;
}

SPI_Status_t SPI_ReceiveByte(SPI_Handle_t *hspi, uint8_t *rx_byte)
{
    // Send dummy byte (0xFF) to clock in one byte from slave
    return SPI_TransferByte(hspi, 0xFF, rx_byte);
}

SPI_Status_t SPI_TransmitBuffer(SPI_Handle_t *hspi, const uint8_t *tx_buffer, size_t length)
{
    if (hspi == NULL || !hspi->is_initialized || tx_buffer == NULL || length == 0) {
        return SPI_STATUS_ERROR;
    }

    SPI_Status_t status;

    // Stream byte sequence over SPI bus
    for (size_t i = 0; i < length; i++) {
        status = SPI_TransferByte(hspi, tx_buffer[i], NULL);
        if (status != SPI_STATUS_OK) {
            return status;
        }
    }

    return SPI_STATUS_OK;
}

SPI_Status_t SPI_ReceiveBuffer(SPI_Handle_t *hspi, uint8_t *rx_buffer, size_t length)
{
    if (hspi == NULL || !hspi->is_initialized || rx_buffer == NULL || length == 0) {
        return SPI_STATUS_ERROR;
    }

    SPI_Status_t status;

    // Read byte sequence into buffer by sending dummy clock pulses
    for (size_t i = 0; i < length; i++) {
        status = SPI_ReceiveByte(hspi, &rx_buffer[i]);
        if (status != SPI_STATUS_OK) {
            return status;
        }
    }

    return SPI_STATUS_OK;
}

// Mutex protecting shared SPI1 bus between OLED and SD Card
static SemaphoreHandle_t s_spi_mutex = NULL;

void SPI_InitMutex(void)
{
    // Create FreeRTOS mutex for bus arbitration
    if (s_spi_mutex == NULL) {
        s_spi_mutex = xSemaphoreCreateMutex();
    }
}

bool SPI_Lock(uint32_t timeout_ms)
{
    // Bypass locking if FreeRTOS scheduler is not active
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED || s_spi_mutex == NULL) {
        return true;
    }
    return (xSemaphoreTake(s_spi_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE);
}

void SPI_Unlock(void)
{
    // Release bus mutex
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED || s_spi_mutex == NULL) {
        return;
    }
    xSemaphoreGive(s_spi_mutex);
}