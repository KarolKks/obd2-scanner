#include "main.h"

int main(void)
{
    BSP_CLK_Init();
    LL_Init1msTick(SystemCoreClock);
    UART_Init();

    CAN_Init();
    UART_SendString("clock 80mhz ,uart and  can works!\r\n");

    while (1)
    {
        UART_SendString("UART is working! \r\n");
        LL_mDelay(1000);
    }
}

