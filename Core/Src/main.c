#include "main.h"


int main(void)
{
    // Initialize core and clocks to 80 MHz
    BSP_CLK_Init();
    LL_Init1msTick(SystemCoreClock);
    
    UART_Init();
    UART_SendString("Start systemu...\r\n");

    if (CAN_Init() != CAN_OK) {
        UART_SendString("Blad inicjalizacji CAN!\r\n");
        while(1);
    }
    
    // Enable filter to receive only OBD2 frames (0x7E8)
    CAN_FilterOBD2();
    UART_SendString("CAN gotowy. Skaner OBD-II dziala.\r\n");

    // Frame requesting engine RPM
    CAN_Frame_t tx_frame = {
        .id = 0x7DF,
        .is_extended = false,
        .is_rtr = false,
        .dlc = 8,
        .data = {0x02, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00}
    };

    CAN_Frame_t rx_frame;
    char buffer[64];

    while (1)
    {
        UART_SendString("Wysylam zapytanie (0x7DF)...\r\n");
        CAN_Transmit(&tx_frame, 50);

        // Give ESP32 time to respond
        LL_mDelay(100);

        if (CAN_IsRxPending()) {
            if (CAN_Receive(&rx_frame) == CAN_OK) {
                // Display ID and first 4 bytes of the response
                sprintf(buffer, "Odebrano ID: 0x%03lX | Dane: %02X %02X %02X %02X\r\n", 
                        (unsigned long)rx_frame.id, 
                        rx_frame.data[0], rx_frame.data[1], 
                        rx_frame.data[2], rx_frame.data[3]);
                UART_SendString(buffer);
            }
        } else {
            UART_SendString("Brak odpowiedzi od ESP32.\r\n");
        }

        LL_mDelay(1000);
    }
}