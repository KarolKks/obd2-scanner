#include "can.h"
#include <stddef.h>

#define CAN_TIMEOUT_INIT_CYCLES   1000000U
#define CAN_TIMEOUT_TX_CYCLES     500000U

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

    // Pull-up on RX line protects against floating input when transceiver is disconnected
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_11, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_12, LL_GPIO_PULL_NO);

    // Exit Sleep mode and enter Initialization mode
    CAN1->MCR &= ~CAN_MCR_SLEEP;
    CAN1->MCR |= CAN_MCR_INRQ;

    uint32_t timeout = CAN_TIMEOUT_INIT_CYCLES;
    while ((CAN1->MSR & CAN_MSR_INAK) == 0) {
        if (--timeout == 0) {
            return CAN_ERR_TIMEOUT;
        }
    }

    // Configure MCR features:
    // ABOM (Automatic Bus-Off Management): automatic recovery from bus-off state
    // AWUM (Automatic Wake-up): automatic wake-up on bus activity
    CAN1->MCR |= CAN_MCR_ABOM | CAN_MCR_AWUM;

    // Configure Bit Timing for Baud Rate = 500 kbps:
    // For PCLK1 = 80 MHz:
    // NBT = 16 tq
    // BRP = 10  (register value: 10 - 1 = 9)
    // TS1 = 13  (register value: 13 - 1 = 12 = 0xC)
    // TS2 = 2   (register value: 2 - 1 = 1 = 0x1)
    // SJW = 1   (register value: 1 - 1 = 0 = 0x0)
    // Sample Point = (1 + 13) / 16 = 87.5% (ISO 11898 / ISO 15765-4 compliant)
    CAN1->BTR = (0U << CAN_BTR_SJW_Pos) |
                (1U << CAN_BTR_TS2_Pos) |
                (12U << CAN_BTR_TS1_Pos) |
                (9U << CAN_BTR_BRP_Pos);

    // Exit Initialization mode and enter Normal mode
    CAN1->MCR &= ~CAN_MCR_INRQ;

    timeout = CAN_TIMEOUT_INIT_CYCLES;
    while ((CAN1->MSR & CAN_MSR_INAK) != 0) {
        if (--timeout == 0) {
            return CAN_ERR_TIMEOUT;
        }
    }

    // Configure default hardware filters: accept all frames (logger / sniffer mode)
    CAN_Status_t filter_status = CAN_FilterAcceptAll();
    if (filter_status != CAN_OK) {
        return filter_status;
    }

    return CAN_OK;
}

CAN_Status_t CAN_Transmit(const CAN_Frame_t *frame, uint32_t timeout_ms)
{
    if (frame == NULL) {
        return CAN_ERR_NULL_PTR;
    }
    if (frame->dlc > 8) {
        return CAN_ERR_PARAM;
    }

    // Check availability of transmit mailboxes (Mailbox 0, 1, or 2)
    uint32_t tsr = CAN1->TSR;
    if ((tsr & (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2)) == 0) {
        return CAN_ERR_BUSY;
    }

    uint8_t mailbox = (uint8_t)((tsr & CAN_TSR_CODE) >> CAN_TSR_CODE_Pos);

    // Format mailbox identifier register
    uint32_t tir = 0;
    if (frame->is_extended) {
        tir = (frame->id << CAN_TI0R_EXID_Pos) | CAN_TI0R_IDE;
    } else {
        tir = ((frame->id & 0x7FF) << CAN_TI0R_STID_Pos);
    }

    if (frame->is_rtr) {
        tir |= CAN_TI0R_RTR;
    }

    // Set Data Length Code (DLC)
    CAN1->sTxMailBox[mailbox].TDTR = frame->dlc & 0x0FU;

    // Write payload bytes
    CAN1->sTxMailBox[mailbox].TDLR = ((uint32_t)frame->data[0]) |
                                    (((uint32_t)frame->data[1]) << 8) |
                                    (((uint32_t)frame->data[2]) << 16) |
                                    (((uint32_t)frame->data[3]) << 24);

    CAN1->sTxMailBox[mailbox].TDHR = ((uint32_t)frame->data[4]) |
                                    (((uint32_t)frame->data[5]) << 8) |
                                    (((uint32_t)frame->data[6]) << 16) |
                                    (((uint32_t)frame->data[7]) << 24);

    // Request transmission
    CAN1->sTxMailBox[mailbox].TIR = tir | CAN_TI0R_TXRQ;

    if (timeout_ms == 0) {
        return CAN_OK;
    }

    // Wait for transmit confirmation (TXOK) with timeout
    uint32_t txok_mask = (CAN_TSR_TXOK0 << (mailbox * 8));
    uint32_t wait_cycles = timeout_ms * (CAN_TIMEOUT_TX_CYCLES / 10);

    while ((CAN1->TSR & txok_mask) == 0) {
        if (--wait_cycles == 0) {
            // Abort transmission on timeout (e.g. no ACK / disconnected bus)
            CAN1->TSR |= (CAN_TSR_ABRQ0 << (mailbox * 8));
            return CAN_ERR_TIMEOUT;
        }
    }

    return CAN_OK;
}

CAN_Status_t CAN_Receive(CAN_Frame_t *frame)
{
    if (frame == NULL) {
        return CAN_ERR_NULL_PTR;
    }

    // Check if at least one frame is pending in FIFO 0
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

    uint32_t rdlr = CAN1->sFIFOMailBox[0].RDLR;
    frame->data[0] = (uint8_t)(rdlr);
    frame->data[1] = (uint8_t)(rdlr >> 8);
    frame->data[2] = (uint8_t)(rdlr >> 16);
    frame->data[3] = (uint8_t)(rdlr >> 24);

    uint32_t rdhr = CAN1->sFIFOMailBox[0].RDHR;
    frame->data[4] = (uint8_t)(rdhr);
    frame->data[5] = (uint8_t)(rdhr >> 8);
    frame->data[6] = (uint8_t)(rdhr >> 16);
    frame->data[7] = (uint8_t)(rdhr >> 24);

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
    // Enter filter initialization mode
    CAN1->FMR |= CAN_FMR_FINIT;

    // Deactivate filter 0 before configuration
    CAN1->FA1R &= ~CAN_FA1R_FACT0;

    // Filter 0: Identifier Mask mode
    CAN1->FM1R &= ~CAN_FM1R_FBM0;

    // Single 32-bit scale for filter 0
    CAN1->FS1R |= CAN_FS1R_FSC0;

    // Assign filter 0 to FIFO 0
    CAN1->FFA1R &= ~CAN_FFA1R_FFA0;

    // Accept all frames: ID = 0, Mask = 0
    CAN1->sFilterRegister[0].FR1 = 0x00000000U;
    CAN1->sFilterRegister[0].FR2 = 0x00000000U;

    // Activate filter 0
    CAN1->FA1R |= CAN_FA1R_FACT0;

    // Exit filter initialization mode
    CAN1->FMR &= ~CAN_FMR_FINIT;

    return CAN_OK;
}

CAN_Status_t CAN_FilterOBD2(void)
{
    CAN1->FMR |= CAN_FMR_FINIT;
    CAN1->FA1R &= ~CAN_FA1R_FACT0;

    // Mask mode, single 32-bit scale, FIFO 0
    CAN1->FM1R &= ~CAN_FM1R_FBM0;
    CAN1->FS1R |= CAN_FS1R_FSC0;
    CAN1->FFA1R &= ~CAN_FFA1R_FFA0;

    // Standard ID 0x7E8 .. 0x7EF (OBD-II ECU responses):
    // Standard ID in 32-bit filter register is shifted left by 21 bits
    // Mask 0x7F8 accepts 0x7E8 through 0x7EF
    CAN1->sFilterRegister[0].FR1 = (0x7E8U << 21);
    CAN1->sFilterRegister[0].FR2 = (0x7F8U << 21);

    CAN1->FA1R |= CAN_FA1R_FACT0;
    CAN1->FMR &= ~CAN_FMR_FINIT;

    return CAN_OK;
}