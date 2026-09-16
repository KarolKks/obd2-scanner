#include "can.h"
#include <stddef.h>

#define CAN_TIMEOUT_LOOPS   100000U

CAN_Status_t CAN_Init(void)
{
    // Enable GPIOA and CAN1 peripheral clocks
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_CAN1);

    // Configure PA11 (CAN_RX) and PA12 (CAN_TX) pins for alternate function AF9
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_11, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_12, LL_GPIO_MODE_ALTERNATE);

    LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_11, LL_GPIO_AF_9);
    LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_12, LL_GPIO_AF_9);

    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_11, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_12, LL_GPIO_SPEED_FREQ_VERY_HIGH);

    // Jawne ustawienie TX jako Push-Pull
    LL_GPIO_SetPinOutputType(GPIOA, LL_GPIO_PIN_12, LL_GPIO_OUTPUT_PUSHPULL);

    // Pull-up na RX line protects against floating input
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_11, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_12, LL_GPIO_PULL_NO);

    // Exit Sleep mode and enter Initialization mode
    CAN1->MCR &= ~CAN_MCR_SLEEP;
    CAN1->MCR |= CAN_MCR_INRQ;

    uint32_t timeout = CAN_TIMEOUT_LOOPS;
    while ((CAN1->MSR & CAN_MSR_INAK) == 0) {
        if (--timeout == 0) {
            return CAN_ERR_TIMEOUT;
        }
    }

    // Configure MCR features: ABOM & AWUM
    CAN1->MCR |= CAN_MCR_ABOM | CAN_MCR_AWUM;

    // Configure Bit Timing for Baud Rate = 500 kbps (PCLK1 = 80 MHz)
    CAN1->BTR = (0U << CAN_BTR_SJW_Pos) |
                (1U << CAN_BTR_TS2_Pos) |
                (12U << CAN_BTR_TS1_Pos) |
                (9U << CAN_BTR_BRP_Pos);

    // Exit Initialization mode and enter Normal mode
    CAN1->MCR &= ~CAN_MCR_INRQ;

    timeout = CAN_TIMEOUT_LOOPS;
    while ((CAN1->MSR & CAN_MSR_INAK) != 0) {
        if (--timeout == 0) {
            return CAN_ERR_TIMEOUT;
        }
    }

    // Configure default hardware filters
    CAN_Status_t filter_status = CAN_FilterAcceptAll();
    if (filter_status != CAN_OK) {
        return filter_status;
    }

    return CAN_OK;
}

CAN_Status_t CAN_Transmit(const CAN_Frame_t *frame, uint32_t timeout_ms)
{
    if (frame == NULL) return CAN_ERR_NULL_PTR;
    if (frame->dlc > 8) return CAN_ERR_PARAM;
    
    if (!frame->is_extended && frame->id > 0x7FFU) return CAN_ERR_PARAM;
    if (frame->is_extended && frame->id > 0x1FFFFFFFU) return CAN_ERR_PARAM;

    // Check availability of transmit mailboxes
    uint32_t tsr = CAN1->TSR;
    if ((tsr & (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2)) == 0) {
        return CAN_ERR_BUSY;
    }

    uint8_t mailbox = (uint8_t)((tsr & CAN_TSR_CODE) >> CAN_TSR_CODE_Pos);
    uint32_t tir = frame->is_extended ? ((frame->id << CAN_TI0R_EXID_Pos) | CAN_TI0R_IDE) 
                                      : ((frame->id & 0x7FFU) << CAN_TI0R_STID_Pos);
    if (frame->is_rtr) tir |= CAN_TI0R_RTR;

    CAN1->sTxMailBox[mailbox].TDTR = frame->dlc & 0x0FU;

    // Kopiowanie danych (optymalizowane)
    CAN1->sTxMailBox[mailbox].TDLR = ((uint32_t)frame->data[0]) | (((uint32_t)frame->data[1]) << 8) |
                                     (((uint32_t)frame->data[2]) << 16) | (((uint32_t)frame->data[3]) << 24);

    CAN1->sTxMailBox[mailbox].TDHR = ((uint32_t)frame->data[4]) | (((uint32_t)frame->data[5]) << 8) |
                                     (((uint32_t)frame->data[6]) << 16) | (((uint32_t)frame->data[7]) << 24);

    // Request transmission
    CAN1->sTxMailBox[mailbox].TIR = tir | CAN_TI0R_TXRQ;

    if (timeout_ms == 0) return CAN_OK;

    uint32_t txok_mask = (CAN_TSR_TXOK0 << (mailbox * 8));
    uint32_t terr_mask = (CAN_TSR_TERR0 << (mailbox * 8));
    uint32_t alst_mask = (CAN_TSR_ALST0 << (mailbox * 8));

    uint32_t timeout = timeout_ms * 10000U;

    // Wait for transmit confirmation
    while ((CAN1->TSR & txok_mask) == 0) {
        // Fast-fail: physical bus error
        if (CAN1->TSR & (terr_mask | alst_mask)) {
            CAN1->TSR |= (CAN_TSR_ABRQ0 << (mailbox * 8)); // Abort
            CAN1->TSR |= (terr_mask | alst_mask);          // Clear error flags
            return CAN_ERR_HARDWARE;
        }

        if (--timeout == 0) {
            CAN1->TSR |= (CAN_TSR_ABRQ0 << (mailbox * 8)); // Abort request
            return CAN_ERR_TIMEOUT;
        }
    }

    // Clear TXOK flag manually for next transfers
    CAN1->TSR |= txok_mask;

    return CAN_OK;
}

CAN_Status_t CAN_Receive(CAN_Frame_t *frame)
{
    if (frame == NULL) return CAN_ERR_NULL_PTR;

    // Check if FIFO Overrun occurred and clear it to unblock receiving
    if (CAN1->RF0R & CAN_RF0R_FOVR0) {
        CAN1->RF0R |= CAN_RF0R_FOVR0;
    }

    if ((CAN1->RF0R & CAN_RF0R_FMP0) == 0) {
        return CAN_ERR_FIFO_EMPTY;
    }

    uint32_t rir = CAN1->sFIFOMailBox[0].RIR;
    if ((rir & CAN_RI0R_IDE) != 0) {
        frame->is_extended = true;
        frame->id = (rir >> CAN_RI0R_EXID_Pos);
    } else {
        frame->is_extended = false;
        frame->id = ((rir >> CAN_RI0R_STID_Pos) & 0x7FFU);
    }

    frame->is_rtr = ((rir & CAN_RI0R_RTR) != 0);
    frame->dlc = (uint8_t)(CAN1->sFIFOMailBox[0].RDTR & CAN_RDT0R_DLC);

    // Copy payload only if it's a data frame
    if (!frame->is_rtr) {
        uint32_t rdlr = CAN1->sFIFOMailBox[0].RDLR;
        uint32_t rdhr = CAN1->sFIFOMailBox[0].RDHR;
        
        for (uint8_t i = 0; i < frame->dlc; i++) {
            if (i < 4) frame->data[i] = (uint8_t)(rdlr >> (i * 8));
            else       frame->data[i] = (uint8_t)(rdhr >> ((i - 4) * 8));
        }
    }

    // Release FIFO 0 output mailbox
    CAN1->RF0R |= CAN_RF0R_RFOM0;

    return CAN_OK;
}

bool CAN_IsRxPending(void)
{
    return (CAN1->RF0R & CAN_RF0R_FMP0) != 0;
}

CAN_Status_t CAN_FilterAcceptAll(void)
{
    CAN1->FMR |= CAN_FMR_FINIT;
    CAN1->FA1R &= ~CAN_FA1R_FACT0;
    CAN1->FM1R &= ~CAN_FM1R_FBM0;
    CAN1->FS1R |= CAN_FS1R_FSC0;
    CAN1->FFA1R &= ~CAN_FFA1R_FFA0;
    CAN1->sFilterRegister[0].FR1 = 0x00000000U;
    CAN1->sFilterRegister[0].FR2 = 0x00000000U;
    CAN1->FA1R |= CAN_FA1R_FACT0;
    CAN1->FMR &= ~CAN_FMR_FINIT;
    return CAN_OK;
}

CAN_Status_t CAN_FilterOBD2(void)
{
    CAN1->FMR |= CAN_FMR_FINIT;
    CAN1->FA1R &= ~CAN_FA1R_FACT0;
    CAN1->FM1R &= ~CAN_FM1R_FBM0;
    CAN1->FS1R |= CAN_FS1R_FSC0;
    CAN1->FFA1R &= ~CAN_FFA1R_FFA0;
    CAN1->sFilterRegister[0].FR1 = (0x7E8U << 21);
    CAN1->sFilterRegister[0].FR2 = (0x7F8U << 21);
    CAN1->FA1R |= CAN_FA1R_FACT0;
    CAN1->FMR &= ~CAN_FMR_FINIT;
    return CAN_OK;
}