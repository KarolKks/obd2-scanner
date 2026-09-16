#include "main.h"

/**
 * @brief Wysyła liczbę całkowitą bez znaku przez UART bez użycia sprintf/printf.
 */
static void UART_SendNumber(uint32_t num)
{
    char buf[11];
    int i = 0;

    if (num == 0) {
        UART_SendString("0");
        return;
    }

    while (num > 0) {
        buf[i++] = (char)('0' + (num % 10));
        num /= 10;
    }

    // Odwrócenie kolejności cyfr w buforze
    char rev[11];
    int j = 0;
    while (i > 0) {
        rev[j++] = buf[--i];
    }
    rev[j] = '\0';

    UART_SendString(rev);
}

/**
 * @brief Wysyła 16-bitowy kod DTC w formacie szesnastkowym (np. P0420).
 */
static void UART_SendDTC(uint16_t dtc)
{
    const char hex_chars[] = "0123456789ABCDEF";
    char dtc_str[6];

    dtc_str[0] = 'P';
    dtc_str[1] = hex_chars[(dtc >> 12) & 0x0F];
    dtc_str[2] = hex_chars[(dtc >> 8) & 0x0F];
    dtc_str[3] = hex_chars[(dtc >> 4) & 0x0F];
    dtc_str[4] = hex_chars[dtc & 0x0F];
    dtc_str[5] = '\0';

    UART_SendString(dtc_str);
}

int main(void)
{
    // Inicjalizacja zegarów i 1ms tick
    BSP_CLK_Init();
    LL_Init1msTick(SystemCoreClock);
    
    UART_Init();
    UART_SendString("\r\n========================================\r\n");
    UART_SendString("    OBD-II DIAGNOSTIC SCANNER READY     \r\n");
    UART_SendString("========================================\r\n");

    if (CAN_Init() != CAN_OK) {
        UART_SendString("Blad inicjalizacji CAN!\r\n");
        while(1);
    }
    
    // Ustawienie filtru CAN na ramki odpowiedzi od ECU (0x7E8)
    CAN_FilterOBD2();
    UART_SendString("Magistrala CAN aktywna (500 kbps).\r\n");

    CAN_Frame_t tx_frame;
    CAN_Frame_t rx_frame;
    OBD_MF_RxContext_t iso_tp_ctx;

    while (1)
    {
        UART_SendString("\r\n--- [1. LIVE DATA (SERVICE 01)] ---\r\n");

        /* --- ODCZYT OBROTÓW SILNIKA (RPM) --- */
        OBD2_BuildRequest(OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_ENGINE_RPM, &tx_frame);
        CAN_Transmit(&tx_frame, 50);
        LL_mDelay(40);

        if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
            if (OBD2_IsResponseValid(&rx_frame, OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_ENGINE_RPM)) {
                float rpm = OBD2_ParseSensorValue(&rx_frame);
                UART_SendString("  -> RPM: ");
                UART_SendNumber((uint32_t)rpm);
                UART_SendString(" obr/min\r\n");
            }
        } else {
            UART_SendString("  -> RPM: Brak odpowiedzi\r\n");
        }

        /* --- ODCZYT PRĘDKOŚCI POJAZDU (SPEED) --- */
        OBD2_BuildRequest(OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_VEHICLE_SPEED, &tx_frame);
        CAN_Transmit(&tx_frame, 50);
        LL_mDelay(40);

        if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
            if (OBD2_IsResponseValid(&rx_frame, OBD2_SERVICE_01_LIVE_DATA, OBD2_PID_VEHICLE_SPEED)) {
                float speed = OBD2_ParseSensorValue(&rx_frame);
                UART_SendString("  -> Speed: ");
                UART_SendNumber((uint32_t)speed);
                UART_SendString(" km/h\r\n");
            }
        } else {
            UART_SendString("  -> Speed: Brak odpowiedzi\r\n");
        }


        UART_SendString("\r\n--- [2. VEHICLE INFO / VIN (SERVICE 09 - ISO-TP)] ---\r\n");

        /* --- ODCZYT NUMERU VIN (MULTI-FRAME) --- */
        OBD_MF_Reset(&iso_tp_ctx);
        OBD2_BuildRequest(OBD2_SERVICE_09_VEHICLE_INFO, 0x02, &tx_frame);
        CAN_Transmit(&tx_frame, 50);

        uint32_t start_time = 0;
        bool vin_received = false;

        while (start_time < 50) {
            if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
                OBD_MF_ProcessFrame(&iso_tp_ctx, &rx_frame);

                if (iso_tp_ctx.state == OBD_MF_STATE_COMPLETE) {
                    char vin[18] = {0};
                    memcpy(vin, &iso_tp_ctx.buffer[3], 17);
                    UART_SendString("  -> VIN odebrany: ");
                    UART_SendString(vin);
                    UART_SendString("\r\n");
                    vin_received = true;
                    break;
                } else if (iso_tp_ctx.state == OBD_MF_STATE_ERROR) {
                    UART_SendString("  -> Blad sekwencji ramek ISO-TP!\r\n");
                    break;
                }
            }
            LL_mDelay(10);
            start_time++;
        }

        if (!vin_received && iso_tp_ctx.state != OBD_MF_STATE_ERROR) {
            UART_SendString("  -> Timeout zapytania o VIN.\r\n");
        }


        UART_SendString("\r\n--- [3. DIAGNOSTIC TROUBLE CODES (SERVICE 03)] ---\r\n");

        /* --- ODCZYT BŁĘDÓW DTC --- */
        OBD_MF_Reset(&iso_tp_ctx);
        OBD2_BuildRequest(OBD2_SERVICE_03_READ_DTC, 0x00, &tx_frame);
        CAN_Transmit(&tx_frame, 50);
        LL_mDelay(60);

        if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
            uint8_t frame_type = rx_frame.data[0] >> 4;

            if (frame_type == 0) {
                uint8_t payload_len = rx_frame.data[0] & 0x0F;
                uint16_t dtc_codes[6];
                uint8_t count = OBD2_ParseDTCs(&rx_frame.data[2], payload_len - 1, dtc_codes, 6);
                
                UART_SendString("  -> Single Frame: znaleziono ");
                UART_SendNumber(count);
                UART_SendString(" DTC\r\n");

                for (uint8_t i = 0; i < count; i++) {
                    UART_SendString("     Kod [");
                    UART_SendNumber(i + 1);
                    UART_SendString("]: ");
                    UART_SendDTC(dtc_codes[i]);
                    UART_SendString("\r\n");
                }
            } else {
                OBD_MF_ProcessFrame(&iso_tp_ctx, &rx_frame);
                start_time = 0;

                while (start_time < 30 && iso_tp_ctx.state == OBD_MF_STATE_RECEIVING) {
                    if (CAN_IsRxPending() && CAN_Receive(&rx_frame) == CAN_OK) {
                        OBD_MF_ProcessFrame(&iso_tp_ctx, &rx_frame);
                    }
                    LL_mDelay(10);
                    start_time++;
                }

                if (iso_tp_ctx.state == OBD_MF_STATE_COMPLETE) {
                    uint16_t dtc_codes[6];
                    uint8_t count = OBD2_ParseDTCs(&iso_tp_ctx.buffer[1], iso_tp_ctx.total_length - 1, dtc_codes, 6);
                    
                    UART_SendString("  -> Multi-Frame: odebrano ");
                    UART_SendNumber(count);
                    UART_SendString(" DTC\r\n");

                    for (uint8_t i = 0; i < count; i++) {
                        UART_SendString("     Kod [");
                        UART_SendNumber(i + 1);
                        UART_SendString("]: ");
                        UART_SendDTC(dtc_codes[i]);
                        UART_SendString("\r\n");
                    }
                }
            }
        } else {
            UART_SendString("  -> Brak odpowiedzi dla Service 03.\r\n");
        }

        UART_SendString("\r\n========================================\r\n");
        LL_mDelay(2000);
    }
}