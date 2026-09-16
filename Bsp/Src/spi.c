#include "spi.h"

#define SPI_TIMEOUT_LOOPS 100000U

SPI_Status_t SPI_Init(SPI_Handle_t *hspi)
{
    // Validate pointer and instance
    if (hspi == NULL || hspi->instance != SPI1) {
        return SPI_STATUS_ERROR;
    }

    // Enable peripheral clocks
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    // Configure GPIOA pins for SPI1 (PA5: SCK, PA6: MISO, PA7: MOSI)
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7, LL_GPIO_MODE_ALTERNATE);
    
    // Assign alternate function 5 (AF5) to all SPI1 pins
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_5, LL_GPIO_AF_5);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_6, LL_GPIO_AF_5);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_7, LL_GPIO_AF_5);

    // Set output speed to very high
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    
    // Enable pull-up on MISO, no pull on SCK and MOSI
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_6, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_5 | LL_GPIO_PIN_7, LL_GPIO_PULL_NO);

    // Configure SPI1 registers
    LL_SPI_SetMode(hspi->instance, LL_SPI_MODE_MASTER);
    LL_SPI_SetStandard(hspi->instance, LL_SPI_PROTOCOL_MOTOROLA);
    LL_SPI_SetDataWidth(hspi->instance, LL_SPI_DATAWIDTH_8BIT);
    LL_SPI_SetTransferBitOrder(hspi->instance, LL_SPI_BIT_ORDER_MSB_FIRST);
    
    // Configure SPI Mode 0 (CPOL=0, CPHA=0)
    LL_SPI_SetClockPolarity(hspi->instance, LL_SPI_POLARITY_LOW);
    LL_SPI_SetClockPhase(hspi->instance, LL_SPI_PHASE_1EDGE);
    
    // Set software NSS management
    LL_SPI_SetNSSMode(hspi->instance, LL_SPI_NSS_SOFT);
    
    // Set baud rate prescaler from the handle
    LL_SPI_SetBaudRatePrescaler(hspi->instance, hspi->baudrate_div);
    
    // Set RX FIFO threshold to 8 bits for byte-by-byte reception
    LL_SPI_SetRxFIFOThreshold(hspi->instance, LL_SPI_RX_FIFO_TH_QUARTER);

    // Enable SPI peripheral and update state
    LL_SPI_Enable(hspi->instance);
    hspi->is_initialized = true;

    return SPI_STATUS_OK;
}

SPI_Status_t SPI_SetBaudrate(SPI_Handle_t *hspi, uint32_t baudrate_div)
{
    // Validate handle and initialization state
    if (hspi == NULL || !hspi->is_initialized) {
        return SPI_STATUS_ERROR;
    }

    uint32_t timeout = SPI_TIMEOUT_LOOPS;

    // Wait until SPI is not busy before disabling it
    while (LL_SPI_IsActiveFlag_BSY(hspi->instance)) {
        if (--timeout == 0) return SPI_STATUS_TIMEOUT;
    }

    // Disable SPI before changing prescaler
    LL_SPI_Disable(hspi->instance);
    
    // Update prescaler register and handle structure
    LL_SPI_SetBaudRatePrescaler(hspi->instance, baudrate_div);
    hspi->baudrate_div = baudrate_div;
    
    // Re-enable SPI peripheral
    LL_SPI_Enable(hspi->instance);

    return SPI_STATUS_OK;
}

SPI_Status_t SPI_TransferByte(SPI_Handle_t *hspi, uint8_t tx_byte, uint8_t *rx_byte)
{
    // Validate pointer and initialization state
    if (hspi == NULL || !hspi->is_initialized) {
        return SPI_STATUS_ERROR;
    }

    uint32_t timeout = SPI_TIMEOUT_LOOPS;

    // Wait until TX buffer is empty
    while (!LL_SPI_IsActiveFlag_TXE(hspi->instance)) {
        if (--timeout == 0) return SPI_STATUS_TIMEOUT;
    }
    
    // Transmit byte
    LL_SPI_TransmitData8(hspi->instance, tx_byte);

    timeout = SPI_TIMEOUT_LOOPS;

    // Wait until RX buffer is not empty
    while (!LL_SPI_IsActiveFlag_RXNE(hspi->instance)) {
        if (--timeout == 0) return SPI_STATUS_TIMEOUT;
    }
    
    // Read received byte
    uint8_t received_data = LL_SPI_ReceiveData8(hspi->instance);

    // Check and clear Overrun (OVR) error flag
    if (LL_SPI_IsActiveFlag_OVR(hspi->instance)) {
        LL_SPI_ClearFlag_OVR(hspi->instance);
        return SPI_STATUS_ERROR;
    }

    // Pass data back if pointer is provided
    if (rx_byte != NULL) {
        *rx_byte = received_data;
    }

    return SPI_STATUS_OK;
}

SPI_Status_t SPI_ReceiveByte(SPI_Handle_t *hspi, uint8_t *rx_byte)
{
    // Send dummy byte (0xFF) to generate clock and receive data
    return SPI_TransferByte(hspi, 0xFF, rx_byte);
}

SPI_Status_t SPI_TransmitBuffer(SPI_Handle_t *hspi, const uint8_t *tx_buffer, size_t length)
{
    // Validate pointers and length
    if (hspi == NULL || !hspi->is_initialized || tx_buffer == NULL || length == 0) {
        return SPI_STATUS_ERROR;
    }

    SPI_Status_t status;

    // Transmit data byte by byte
    for (size_t i = 0; i < length; i++) {
        // Pass NULL for rx_byte since we only care about transmitting
        status = SPI_TransferByte(hspi, tx_buffer[i], NULL);
        if (status != SPI_STATUS_OK) {
            return status; // Abort on first hardware error or timeout
        }
    }

    return SPI_STATUS_OK;
}

SPI_Status_t SPI_ReceiveBuffer(SPI_Handle_t *hspi, uint8_t *rx_buffer, size_t length)
{
    // Validate pointers and length
    if (hspi == NULL || !hspi->is_initialized || rx_buffer == NULL || length == 0) {
        return SPI_STATUS_ERROR;
    }

    SPI_Status_t status;

    // Receive data byte by byte
    for (size_t i = 0; i < length; i++) {
        status = SPI_ReceiveByte(hspi, &rx_buffer[i]);
        if (status != SPI_STATUS_OK) {
            return status; // Abort on first hardware error or timeout
        }
    }

    return SPI_STATUS_OK;
}