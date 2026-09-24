#include "can.h"
#include "clk.h"

#define CAN_INIT_TIMEOUT_MS 100U
#define CAN_RX_QUEUE_SIZE   16U

// FreeRTOS queue storing incoming CAN frames dispatched by ISR
static QueueHandle_t s_can_rx_queue = NULL;

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

    // Set TX pin as Push-Pull output
    LL_GPIO_SetPinOutputType(GPIOA, LL_GPIO_PIN_12, LL_GPIO_OUTPUT_PUSHPULL);

    // Pull-up on RX line protects against floating bus input
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_11, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_12, LL_GPIO_PULL_NO);

    // Exit Sleep mode and enter Initialization mode
    CAN1->MCR &= ~CAN_MCR_SLEEP;
    CAN1->MCR |= CAN_MCR_INRQ;

    // Wait until CAN controller confirms entry into initialization mode
    uint32_t start_tick = CLK_GetTick();
    while ((CAN1->MSR & CAN_MSR_INAK) == 0) {
        if ((CLK_GetTick() - start_tick) >= CAN_INIT_TIMEOUT_MS) {
            return CAN_ERR_TIMEOUT;
        }
    }

    // Enable Automatic Bus-Off Management (ABOM) and Automatic Wake-Up Mode (AWUM)
    CAN1->MCR |= CAN_MCR_ABOM | CAN_MCR_AWUM;

    // Configure Bit Timing for 500 kbps (PCLK1 = 80 MHz, BRP=10, TS1=13, TS2=2, SJW=1)
    CAN1->BTR = (0U << CAN_BTR_SJW_Pos) |
                (1U << CAN_BTR_TS2_Pos) |
                (12U << CAN_BTR_TS1_Pos) |
                (9U << CAN_BTR_BRP_Pos);

    // Request exit from Initialization mode to enter Normal active mode
    CAN1->MCR &= ~CAN_MCR_INRQ;

    // Wait until CAN controller completes transition to normal operating mode
    start_tick = CLK_GetTick();
    while ((CAN1->MSR & CAN_MSR_INAK) != 0) {
        if ((CLK_GetTick() - start_tick) >= CAN_INIT_TIMEOUT_MS) {
            return CAN_ERR_TIMEOUT;
        }
    }

    // Configure default hardware acceptance filters (accept all IDs)
    CAN_Status_t filter_status = CAN_FilterAcceptAll();
    if (filter_status != CAN_OK) {
        return filter_status;
    }

    // Create FreeRTOS receive queue if not already initialized
    if (s_can_rx_queue == NULL) {
        s_can_rx_queue = xQueueCreate(CAN_RX_QUEUE_SIZE, sizeof(CAN_Frame_t));
    }

    // Enable FIFO 0 message pending interrupt (FMPIE0)
    CAN1->IER |= CAN_IER_FMPIE0;

    // Configure NVIC priority 6 for CAN1_RX0_IRQn (safe for FreeRTOS MAX_SYSCALL_PRIORITY = 5)
    NVIC_SetPriority(CAN1_RX0_IRQn, 6);
    NVIC_EnableIRQ(CAN1_RX0_IRQn);

    return CAN_OK;
}

CAN_Status_t CAN_Transmit(const CAN_Frame_t *frame, uint32_t timeout_ms)
{
    if (frame == NULL) return CAN_ERR_NULL_PTR;
    if (frame->dlc > 8) return CAN_ERR_PARAM;
    
    // Validate identifier bounds for standard (11-bit) and extended (29-bit) frames
    if (!frame->is_extended && frame->id > 0x7FFU) return CAN_ERR_PARAM;
    if (frame->is_extended && frame->id > 0x1FFFFFFFU) return CAN_ERR_PARAM;

    // Wait until at least one hardware transmit mailbox is empty
    uint32_t start_tick = CLK_GetTick();
    uint32_t tsr = CAN1->TSR;
    while ((tsr & (CAN_TSR_TME0 | CAN_TSR_TME1 | CAN_TSR_TME2)) == 0) {
        if (timeout_ms == 0 || (CLK_GetTick() - start_tick) >= timeout_ms) {
            return CAN_ERR_BUSY;
        }
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
            taskYIELD();
        }
        tsr = CAN1->TSR;
    }

    // Identify the available transmit mailbox
    uint8_t mailbox = (uint8_t)((tsr & CAN_TSR_CODE) >> CAN_TSR_CODE_Pos);

    // Format standard/extended identifier and remote transmission request (RTR) flag
    uint32_t tir = frame->is_extended ? ((frame->id << CAN_TI0R_EXID_Pos) | CAN_TI0R_IDE) 
                                      : ((frame->id & 0x7FFU) << CAN_TI0R_STID_Pos);
    if (frame->is_rtr) tir |= CAN_TI0R_RTR;

    // Set Data Length Code (DLC)
    CAN1->sTxMailBox[mailbox].TDTR = frame->dlc & 0x0FU;

    // Load payload bytes into lower and higher 32-bit data registers
    CAN1->sTxMailBox[mailbox].TDLR = ((uint32_t)frame->data[0]) | (((uint32_t)frame->data[1]) << 8) |
                                     (((uint32_t)frame->data[2]) << 16) | (((uint32_t)frame->data[3]) << 24);

    CAN1->sTxMailBox[mailbox].TDHR = ((uint32_t)frame->data[4]) | (((uint32_t)frame->data[5]) << 8) |
                                     (((uint32_t)frame->data[6]) << 16) | (((uint32_t)frame->data[7]) << 24);

    // Request transmission on selected mailbox
    CAN1->sTxMailBox[mailbox].TIR = tir | CAN_TI0R_TXRQ;

    if (timeout_ms == 0) return CAN_OK;

    uint32_t txok_mask = (CAN_TSR_TXOK0 << (mailbox * 8));
    uint32_t terr_mask = (CAN_TSR_TERR0 << (mailbox * 8));
    uint32_t alst_mask = (CAN_TSR_ALST0 << (mailbox * 8));

    start_tick = CLK_GetTick();

    // Wait for hardware transmission confirmation or transmission error
    while ((CAN1->TSR & txok_mask) == 0) {
        // Fast-fail: physical bus error or arbitration lost
        if (CAN1->TSR & (terr_mask | alst_mask)) {
            CAN1->TSR |= (CAN_TSR_ABRQ0 << (mailbox * 8)); // Abort mailbox transmission
            CAN1->TSR |= (terr_mask | alst_mask);          // Clear error status flags
            return CAN_ERR_HARDWARE;
        }

        if ((CLK_GetTick() - start_tick) >= timeout_ms) {
            CAN1->TSR |= (CAN_TSR_ABRQ0 << (mailbox * 8)); // Abort on timeout
            return CAN_ERR_TIMEOUT;
        }

        // Yield CPU time to other tasks while waiting for CAN bus ACK
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
            taskYIELD();
        }
    }

    // Clear TXOK flag for subsequent transfers
    CAN1->TSR |= txok_mask;

    return CAN_OK;
}

void CAN1_RX0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Clear FIFO overrun flag if set to prevent receiver blockage
    if (CAN1->RF0R & CAN_RF0R_FOVR0) {
        CAN1->RF0R |= CAN_RF0R_FOVR0;
    }

    // Drain all pending frames from hardware FIFO 0 (capacity: 3 frames)
    while ((CAN1->RF0R & CAN_RF0R_FMP0) != 0) {
        CAN_Frame_t frame;
        uint32_t rir = CAN1->sFIFOMailBox[0].RIR;

        // Parse identifier type and ID value
        if ((rir & CAN_RI0R_IDE) != 0) {
            frame.is_extended = true;
            frame.id = (rir >> CAN_RI0R_EXID_Pos);
        } else {
            frame.is_extended = false;
            frame.id = ((rir >> CAN_RI0R_STID_Pos) & 0x7FFU);
        }

        frame.is_rtr = ((rir & CAN_RI0R_RTR) != 0);
        frame.dlc = (uint8_t)(CAN1->sFIFOMailBox[0].RDTR & CAN_RDT0R_DLC);

        // Unpack payload data bytes from 32-bit FIFO registers
        if (!frame.is_rtr) {
            uint32_t rdlr = CAN1->sFIFOMailBox[0].RDLR;
            uint32_t rdhr = CAN1->sFIFOMailBox[0].RDHR;
            for (uint8_t i = 0; i < frame.dlc; i++) {
                if (i < 4) frame.data[i] = (uint8_t)(rdlr >> (i * 8));
                else       frame.data[i] = (uint8_t)(rdhr >> ((i - 4) * 8));
            }
        }

        // Release hardware mailbox in FIFO 0 for new frames
        CAN1->RF0R |= CAN_RF0R_RFOM0;

        // Forward received frame to FreeRTOS queue if OS scheduler is running
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
            if (s_can_rx_queue != NULL) {
                xQueueSendFromISR(s_can_rx_queue, &frame, &xHigherPriorityTaskWoken);
            }
        }
    }

    // Context switch if a higher priority task was awakened by the received frame
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

CAN_Status_t CAN_Receive(CAN_Frame_t *frame, uint32_t timeout_ms)
{
    if (frame == NULL) return CAN_ERR_NULL_PTR;
    if (s_can_rx_queue == NULL) return CAN_ERR_FIFO_EMPTY;

    // Dequeue frame from FreeRTOS queue with timeout
    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    if (xQueueReceive(s_can_rx_queue, frame, ticks) == pdPASS) {
        return CAN_OK;
    }

    return CAN_ERR_TIMEOUT;
}

bool CAN_IsRxPending(void)
{
    // Return true if unread frames are currently waiting in the receive queue
    if (s_can_rx_queue == NULL) return false;
    return (uxQueueMessagesWaiting(s_can_rx_queue) > 0);
}

CAN_Status_t CAN_FilterAcceptAll(void)
{
    // Enter filter initialization mode
    CAN1->FMR |= CAN_FMR_FINIT;

    // Deactivate filter bank 0 during reconfiguration
    CAN1->FA1R &= ~CAN_FA1R_FACT0;

    // Configure filter 0: Identifier Mask mode (FBM=0), Single 32-bit scale (FSC=1), Assign to FIFO 0 (FFA=0)
    CAN1->FM1R &= ~CAN_FM1R_FBM0;
    CAN1->FS1R |= CAN_FS1R_FSC0;
    CAN1->FFA1R &= ~CAN_FFA1R_FFA0;

    // ID = 0x00, Mask = 0x00 (allows every CAN message through)
    CAN1->sFilterRegister[0].FR1 = 0x00000000U;
    CAN1->sFilterRegister[0].FR2 = 0x00000000U;

    // Activate filter bank 0 and exit filter initialization mode
    CAN1->FA1R |= CAN_FA1R_FACT0;
    CAN1->FMR &= ~CAN_FMR_FINIT;
    return CAN_OK;
}

CAN_Status_t CAN_FilterOBD2(void)
{
    // Enter filter initialization mode
    CAN1->FMR |= CAN_FMR_FINIT;

    // Deactivate filter bank 0 during reconfiguration
    CAN1->FA1R &= ~CAN_FA1R_FACT0;

    // Configure filter 0: Identifier Mask mode (FBM=0), Single 32-bit scale (FSC=1), Assign to FIFO 0 (FFA=0)
    CAN1->FM1R &= ~CAN_FM1R_FBM0;
    CAN1->FS1R |= CAN_FS1R_FSC0;
    CAN1->FFA1R &= ~CAN_FFA1R_FFA0;

    // Filter for standard OBD-II response ID range (0x7E8 to 0x7EF via base 0x7E8 and mask 0x7F8)
    CAN1->sFilterRegister[0].FR1 = (0x7E8U << 21);
    CAN1->sFilterRegister[0].FR2 = (0x7F8U << 21);

    // Activate filter bank 0 and exit filter initialization mode
    CAN1->FA1R |= CAN_FA1R_FACT0;
    CAN1->FMR &= ~CAN_FMR_FINIT;
    return CAN_OK;
}

void CAN_FlushRxQueue(void)
{
    // Reset FreeRTOS queue to discard any stale frames
    if (s_can_rx_queue != NULL) {
        xQueueReset(s_can_rx_queue);
    }
}