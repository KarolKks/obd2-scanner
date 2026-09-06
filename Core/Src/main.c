#include "main.h"

int main(void)
{
    LL_Init1msTick(SystemCoreClock);

    UART_Init();

    /* Infinite loop */
    while (1)
    {
        UART_SendString("UART is working! \r\n");
        LL_mDelay(1000);
    }
}
