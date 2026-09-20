#include "ky040.h"

#define KY040_QUEUE_CAPACITY      16U
#define KY040_STEPS_PER_DETENT    2       /* In X2 mode, KY-040 generates 2 counter ticks per physical detent */
#define BUTTON_DEBOUNCE_MS        15U     /* Minimum press duration to filter mechanical contact chatter */
#define BUTTON_HOLD_THRESHOLD_MS  600U    /* Threshold in ms to differentiate Short Click from Long Hold */

static QueueHandle_t s_ky040_queue = NULL;
static int16_t s_last_counter = 0;

static uint32_t s_press_duration_ms = 0;
static bool s_hold_reported = false;
static TickType_t s_last_update_tick = 0;

KY040_Status_t KY040_Init(void)
{
    // Initialize low-level hardware timer (TIM3 PB4/PB5) and button GPIO (PA10 / D2)
    if (TIM3_Encoder_Init() != TIM_OK || GPIO_Init() != GPIO_OK) {
        return KY040_ERR_INIT;
    }

    s_last_counter = 0;
    s_press_duration_ms = 0;
    s_hold_reported = false;
    s_last_update_tick = xTaskGetTickCount();

    // Create FreeRTOS navigation event queue
    if (s_ky040_queue == NULL) {
        s_ky040_queue = xQueueCreate(KY040_QUEUE_CAPACITY, sizeof(KY040_Event_t));
        if (s_ky040_queue == NULL) {
            return KY040_ERR_INIT;
        }
    }

    return KY040_OK;
}

void KY040_Update(void)
{
    if (s_ky040_queue == NULL) return;

    TickType_t current_tick = xTaskGetTickCount();
    uint32_t elapsed_ms = (uint32_t)((current_tick - s_last_update_tick) * portTICK_PERIOD_MS);
    s_last_update_tick = current_tick;
    if (elapsed_ms == 0) elapsed_ms = 1;

    // Process hardware timer counter for detents
    int16_t current_cnt = (int16_t)TIM3_Encoder_GetCount();
    int16_t delta = (int16_t)(current_cnt - s_last_counter);

    // Each physical detent generates KY040_STEPS_PER_DETENT counts
    while (delta >= KY040_STEPS_PER_DETENT) {
        KY040_Event_t ev = KY040_EVENT_CW;
        xQueueSend(s_ky040_queue, &ev, 0);
        delta -= KY040_STEPS_PER_DETENT;
        s_last_counter += KY040_STEPS_PER_DETENT;
    }

    while (delta <= -KY040_STEPS_PER_DETENT) {
        KY040_Event_t ev = KY040_EVENT_CCW;
        xQueueSend(s_ky040_queue, &ev, 0);
        delta += KY040_STEPS_PER_DETENT;
        s_last_counter -= KY040_STEPS_PER_DETENT;
    }

    // Process button state with debouncing
    bool is_pressed = GPIO_Button_IsPressed();

    if (is_pressed) {
        s_press_duration_ms += elapsed_ms;

        // Trigger Long Hold event once threshold is reached
        if (s_press_duration_ms >= BUTTON_HOLD_THRESHOLD_MS && !s_hold_reported) {
            s_hold_reported = true;
            KY040_Event_t ev = KY040_EVENT_HOLD;
            xQueueSend(s_ky040_queue, &ev, 0);
        }
    } else {
        // Button released: check if it qualifies as a Short Click
        if (s_press_duration_ms >= BUTTON_DEBOUNCE_MS && !s_hold_reported) {
            KY040_Event_t ev = KY040_EVENT_CLICK;
            xQueueSend(s_ky040_queue, &ev, 0);
        }

        s_press_duration_ms = 0;
        s_hold_reported = false;
    }
}

bool KY040_GetEvent(KY040_Event_t *event, uint32_t timeout_ms)
{
    if (event == NULL || s_ky040_queue == NULL) {
        return false;
    }

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    return (xQueueReceive(s_ky040_queue, event, ticks) == pdPASS);
}

int16_t KY040_GetRawDelta(void)
{
    int16_t current = (int16_t)TIM3_Encoder_GetCount();
    int16_t diff = (int16_t)(current - s_last_counter);
    s_last_counter = current;
    return diff;
}
