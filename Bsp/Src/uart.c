#include "uart.h"

#define UART_RX_BUFFER_SIZE  128U

static volatile uint8_t s_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint16_t s_rx_head = 0;
static volatile uint16_t s_rx_tail = 0;

static UART_RxCallback_t s_rx_callback = NULL;

UART_Status_t UART_Init(void)
{
    // Enable GPIOA and USART2 peripheral clocks
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);

    // Configure PA2 (TX) for alternate function AF7
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_2, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_2, LL_GPIO_AF_7);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_2, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_2, LL_GPIO_PULL_NO);

    // Configure PA3 (RX) for alternate function AF7
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_3, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_3, LL_GPIO_AF_7);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_3, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_3, LL_GPIO_PULL_UP);

    // Configure USART2 parameters: 115200 baud, 8 data bits, no parity, 1 stop bit
    LL_USART_Disable(USART2);
    LL_USART_ConfigCharacter(USART2, LL_USART_DATAWIDTH_8B, LL_USART_PARITY_NONE, LL_USART_STOPBITS_1);
    LL_USART_SetBaudRate(USART2, SystemCoreClock, LL_USART_OVERSAMPLING_16, 115200);
    LL_USART_SetTransferDirection(USART2, LL_USART_DIRECTION_TX_RX);

    LL_USART_Enable(USART2);

    // Configure RXNE interrupt
    // Priority 6 is safe for FreeRTOS (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5)
    NVIC_SetPriority(USART2_IRQn, 6);
    NVIC_EnableIRQ(USART2_IRQn);

    LL_USART_EnableIT_RXNE(USART2);

    return UART_OK;
}

UART_Status_t UART_SendChar(char ch)
{
    while (LL_USART_IsActiveFlag_TXE(USART2) == 0) {
        // Wait until transmit data register is empty
    }
    LL_USART_TransmitData8(USART2, (uint8_t)ch);
    return UART_OK;
}

UART_Status_t UART_SendString(const char* str)
{
    if (str == NULL) {
        return UART_ERR_NULL_PTR;
    }

    uint32_t i = 0;
    while (str[i] != '\0') {
        UART_SendChar(str[i]);
        i++;
    }

    return UART_OK;
}

UART_Status_t UART_RegisterRxCallback(UART_RxCallback_t callback)
{
    s_rx_callback = callback;
    return UART_OK;
}

bool UART_IsRxDataAvailable(void)
{
    return (s_rx_head != s_rx_tail);
}

UART_Status_t UART_ReadChar(char *out_char)
{
    if (out_char == NULL) {
        return UART_ERR_NULL_PTR;
    }

    if (s_rx_head == s_rx_tail) {
        return UART_ERR_EMPTY;
    }

    *out_char = (char)s_rx_buffer[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1) % UART_RX_BUFFER_SIZE);

    return UART_OK;
}

void UART_IRQHandler(void)
{
    // Clear error flags to prevent blocking reception
    if (LL_USART_IsActiveFlag_ORE(USART2)) {
        LL_USART_ClearFlag_ORE(USART2);
    }
    if (LL_USART_IsActiveFlag_FE(USART2)) {
        LL_USART_ClearFlag_FE(USART2);
    }
    if (LL_USART_IsActiveFlag_NE(USART2)) {
        LL_USART_ClearFlag_NE(USART2);
    }

    // Handle received byte
    if (LL_USART_IsActiveFlag_RXNE(USART2)) {
        uint8_t data = LL_USART_ReceiveData8(USART2);

        // Store into circular buffer
        uint16_t next_head = (uint16_t)((s_rx_head + 1) % UART_RX_BUFFER_SIZE);
        if (next_head != s_rx_tail) {
            s_rx_buffer[s_rx_head] = data;
            s_rx_head = next_head;
        }

        // Call user callback if registered
        if (s_rx_callback != NULL) {
            s_rx_callback(data);
        }
    }
}

/**
 * @brief USART2 hardware interrupt handler defined in startup vector table.
 */
void USART2_IRQHandler(void)
{
    UART_IRQHandler();
}
