#include "uart.h"

#define UART_RX_BUFFER_SIZE  128U

// Software circular FIFO buffer for incoming UART bytes
static volatile uint8_t s_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint16_t s_rx_head = 0;
static volatile uint16_t s_rx_tail = 0;

// Optional asynchronous receive callback
static UART_RxCallback_t s_rx_callback = NULL;

UART_Status_t UART_Init(void)
{
    // Enable GPIOA and USART2 peripheral clocks
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);

    // Configure PA2 (TX) for Alternate Function AF7 (USART2_TX)
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_2, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_2, LL_GPIO_AF_7);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_2, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_2, LL_GPIO_PULL_NO);

    // Configure PA3 (RX) for Alternate Function AF7 (USART2_RX) with pull-up
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_3, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_3, LL_GPIO_AF_7);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_3, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_3, LL_GPIO_PULL_UP);

    // Configure USART2: 115200 baud, 8 data bits, no parity, 1 stop bit, 16x oversampling
    LL_USART_Disable(USART2);
    LL_USART_ConfigCharacter(USART2, LL_USART_DATAWIDTH_8B, LL_USART_PARITY_NONE, LL_USART_STOPBITS_1);
    LL_USART_SetBaudRate(USART2, SystemCoreClock, LL_USART_OVERSAMPLING_16, 115200);
    LL_USART_SetTransferDirection(USART2, LL_USART_DIRECTION_TX_RX);

    LL_USART_Enable(USART2);

    // Configure NVIC priority 6 for USART2 (safe for FreeRTOS MAX_SYSCALL_PRIORITY = 5)
    NVIC_SetPriority(USART2_IRQn, 6);
    NVIC_EnableIRQ(USART2_IRQn);

    // Enable RX Not Empty (RXNE) interrupt
    LL_USART_EnableIT_RXNE(USART2);

    return UART_OK;
}

UART_Status_t UART_SendChar(char ch)
{
    if (!LL_USART_IsEnabled(USART2)) {
        return UART_ERR_BUSY;
    }

    // Wait until transmit data register is empty
    uint32_t timeout = 100000U;
    while (LL_USART_IsActiveFlag_TXE(USART2) == 0) {
        if (--timeout == 0) {
            return UART_ERR_TIMEOUT;
        }
    }

    // Write character into TX data register
    LL_USART_TransmitData8(USART2, (uint8_t)ch);
    return UART_OK;
}

UART_Status_t UART_SendString(const char* str)
{
    if (str == NULL) {
        return UART_ERR_NULL_PTR;
    }

    // Transmit null-terminated string character by character
    uint32_t i = 0;
    while (str[i] != '\0') {
        UART_Status_t status = UART_SendChar(str[i]);
        if (status != UART_OK) {
            return status;
        }
        i++;
    }

    return UART_OK;
}

UART_Status_t UART_SendNumber(uint32_t num)
{
    char buf[11];
    int i = 0;

    if (num == 0) {
        return UART_SendString("0");
    }

    // Convert integer to reversed decimal ASCII string
    while (num > 0) {
        buf[i++] = (char)('0' + (num % 10));
        num /= 10;
    }

    // Reverse character sequence for transmission
    char rev[11];
    int j = 0;
    while (i > 0) {
        rev[j++] = buf[--i];
    }
    rev[j] = '\0';

    return UART_SendString(rev);
}

UART_Status_t UART_RegisterRxCallback(UART_RxCallback_t callback)
{
    // Register asynchronous ISR byte-received callback
    s_rx_callback = callback;
    return UART_OK;
}

bool UART_IsRxDataAvailable(void)
{
    // Return true if circular buffer contains unread bytes
    return (s_rx_head != s_rx_tail);
}

UART_Status_t UART_ReadChar(char *out_char)
{
    if (out_char == NULL) {
        return UART_ERR_NULL_PTR;
    }

    // Check if circular buffer is empty
    if (s_rx_head == s_rx_tail) {
        return UART_ERR_EMPTY;
    }

    // Pop oldest byte from circular buffer
    *out_char = (char)s_rx_buffer[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1) % UART_RX_BUFFER_SIZE);

    return UART_OK;
}

void UART_IRQHandler(void)
{
    // Clear overrun (ORE), framing (FE), and noise (NE) error flags to unblock receiver
    if (LL_USART_IsActiveFlag_ORE(USART2)) {
        LL_USART_ClearFlag_ORE(USART2);
    }
    if (LL_USART_IsActiveFlag_FE(USART2)) {
        LL_USART_ClearFlag_FE(USART2);
    }
    if (LL_USART_IsActiveFlag_NE(USART2)) {
        LL_USART_ClearFlag_NE(USART2);
    }

    // Process incoming byte when RXNE flag is active
    if (LL_USART_IsActiveFlag_RXNE(USART2)) {
        uint8_t data = LL_USART_ReceiveData8(USART2);

        // Push byte into circular ring buffer
        uint16_t next_head = (uint16_t)((s_rx_head + 1) % UART_RX_BUFFER_SIZE);
        if (next_head != s_rx_tail) {
            s_rx_buffer[s_rx_head] = data;
            s_rx_head = next_head;
        }

        // Trigger user callback if installed
        if (s_rx_callback != NULL) {
            s_rx_callback(data);
        }
    }
}

void USART2_IRQHandler(void)
{
    // Forward hardware vector interrupt to handler
    UART_IRQHandler();
}
