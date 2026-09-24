#include "task_ui.h"
#include "ui_common.h"

// FreeRTOS UI Presentation Task configuration
#define UI_TASK_STACK_SIZE      768U
#define UI_TASK_PRIORITY        (tskIDLE_PRIORITY + 1)

// UI task state and communication buffers
static QueueHandle_t s_telemetry_queue = NULL;  // Depth-1 queue for latest vehicle snapshot
static UIMode_t      s_ui_mode = UI_MODE_MENU;  // Root menu (UI_MODE_MENU) or active screen (UI_MODE_SUBVIEW)
static int8_t        s_menu_idx = 0;            // Currently highlighted main menu item index
static int8_t        s_menu_scroll = 0;         // Viewport vertical scroll offset (top visible row)

// Switches view state back to the root diagnostic menu
void UI_ExitToMenu(void)
{
    s_ui_mode = UI_MODE_MENU;
}

// Standardized header bar: title string at top with separator line
void UI_RenderHeader(const char *title)
{
    SH1106_DrawString(2, 2, title, &Font_6x8, SH1106_COLOR_WHITE);
    SH1106_DrawLine(0, 10, 127, 10, SH1106_COLOR_WHITE);
}

// Standardized footer bar: separator line with contextual navigation hints
void UI_RenderFooter(const char *text)
{
    SH1106_DrawLine(0, 52, 127, 52, SH1106_COLOR_WHITE);
    SH1106_DrawString(2, 54, text, &Font_6x8, SH1106_COLOR_WHITE);
}

// Animated startup splash screen with progressive loading bar
static void Task_UI_ShowSplash(void)
{
    for (uint8_t step = 0; step <= 5; step++) {
        SH1106_Clear();
        SH1106_DrawRect(0, 0, 127, 63, SH1106_COLOR_WHITE);
        SH1106_DrawString(14, 8, "OBD-II SCANNER", &Font_6x8, SH1106_COLOR_WHITE);
        SH1106_DrawString(14, 20, "DIAGNOSTIC TOOL", &Font_6x8, SH1106_COLOR_WHITE);
        SH1106_DrawString(14, 32, "CAN 500k STM32L4", &Font_6x8, SH1106_COLOR_WHITE);

        // Draw outer box and fill animated progress bar
        SH1106_DrawRect(14, 46, 100, 8, SH1106_COLOR_WHITE);
        uint8_t fill_w = (uint8_t)(step * 20);
        if (fill_w > 0) {
            SH1106_FillRect(14, 46, fill_w, 8, SH1106_COLOR_WHITE);
        }

        SH1106_UpdateScreen();
        vTaskDelay(pdMS_TO_TICKS(180));
    }
}

// Registered screen descriptors forming the main diagnostic menu
static const UI_Screen_t * const s_screens[] = {
    &g_screen_live,
    &g_screen_dtc,
    &g_screen_clear_dtc,
    &g_screen_freeze_frame,
    &g_screen_vehicle_info,
    &g_screen_protocol_info,
    &g_screen_sd_interval,
    &g_screen_sd_channels,
    &g_screen_set_datetime,
};

#define MENU_SCREEN_COUNT ((int8_t)(sizeof(s_screens) / sizeof(s_screens[0])))

// Renders the main scrollable diagnostic menu on the OLED display
static void Task_UI_RenderMainMenu(void)
{
    UI_RenderHeader("[ MAIN DIAGNOSTIC MENU ]");

    // Render 4 visible menu items inside current scroll window
    for (uint8_t row = 0; row < 4; row++) {
        int8_t item_idx = s_menu_scroll + row;
        if (item_idx >= MENU_SCREEN_COUNT) break;

        int16_t y = 12 + (row * 10);
        if (item_idx == s_menu_idx) {
            // Invert colors to highlight active menu selection
            SH1106_FillRect(0, y - 1, 120, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(2, y, s_screens[item_idx]->label, &Font_6x8, SH1106_COLOR_BLACK);
        } else {
            SH1106_DrawString(2, y, s_screens[item_idx]->label, &Font_6x8, SH1106_COLOR_WHITE);
        }
    }

    // Draw up / down arrows if items exist beyond current 4-row viewport
    if (s_menu_scroll > 0) {
        SH1106_DrawString(122, 12, "^", &Font_6x8, SH1106_COLOR_WHITE);
    }
    if ((s_menu_scroll + 4) < MENU_SCREEN_COUNT) {
        SH1106_DrawString(122, 42, "v", &Font_6x8, SH1106_COLOR_WHITE);
    }

    // Display view-specific dynamic footer or default navigation guidance
    const char *footer = NULL;
    if (s_screens[s_menu_idx]->get_footer != NULL) {
        footer = s_screens[s_menu_idx]->get_footer();
    }
    if (footer != NULL) {
        UI_RenderFooter(footer);
    } else {
        UI_RenderFooter("Rotate:Select Click:OK");
    }
}

// Master rendering dispatcher: draws menu or delegates to active subview
static void Task_UI_Render(const VehicleData_t *data)
{
    SH1106_Clear();

    if (s_ui_mode == UI_MODE_MENU) {
        Task_UI_RenderMainMenu();
    } else {
        if (s_screens[s_menu_idx]->render != NULL) {
            s_screens[s_menu_idx]->render(data);
        }
    }

    // Push local framebuffer to physical SH1106 OLED controller over SPI
    SH1106_UpdateScreen();
}

// FreeRTOS UI Presentation Task main execution loop
static void Task_UI_Body(void *argument)
{
    (void)argument;
    VehicleData_t data;
    memset(&data, 0, sizeof(VehicleData_t));

    // Show animated startup splash screen
    Task_UI_ShowSplash();

    TickType_t last_oled_render = 0;
    bool needs_redraw = true;

    while (1)
    {
        // Poll encoder hardware timer and push events to internal buffer
        KY040_Update();

        // Process all pending rotary encoder events from queue
        KY040_Event_t enc_event;
        while (KY040_GetEvent(&enc_event, 0)) {
            if (s_ui_mode == UI_MODE_MENU) {
                // Menu navigation: rotate moves cursor, click opens subview
                switch (enc_event) {
                    case KY040_EVENT_CW:
                        if (s_menu_idx < (MENU_SCREEN_COUNT - 1)) {
                            s_menu_idx++;
                            if (s_menu_idx >= (s_menu_scroll + 4)) {
                                s_menu_scroll = s_menu_idx - 3;
                            }
                        }
                        break;
                    case KY040_EVENT_CCW:
                        if (s_menu_idx > 0) {
                            s_menu_idx--;
                            if (s_menu_idx < s_menu_scroll) {
                                s_menu_scroll = s_menu_idx;
                            }
                        }
                        break;
                    case KY040_EVENT_CLICK:
                        s_ui_mode = UI_MODE_SUBVIEW;
                        if (s_screens[s_menu_idx]->on_enter != NULL) {
                            s_screens[s_menu_idx]->on_enter();
                        }
                        break;
                    default:
                        break;
                }
            } else {
                // Active subview event handling: hold exits to menu by default
                if (enc_event == KY040_EVENT_HOLD) {
                    if (s_screens[s_menu_idx]->on_event != NULL) {
                        s_screens[s_menu_idx]->on_event(KY040_EVENT_HOLD, &data);
                    } else {
                        UI_ExitToMenu();
                    }
                } else if (s_screens[s_menu_idx]->on_event != NULL) {
                    s_screens[s_menu_idx]->on_event(enc_event, &data);
                }
            }
            needs_redraw = true;
        }

        // Receive latest telemetry snapshot from OBD-II task (non-blocking)
        if (xQueueReceive(s_telemetry_queue, &data, pdMS_TO_TICKS(10)) == pdPASS) {
            needs_redraw = true;
        }

        // Redraw display immediately on event, or on 80ms periodic timer (~12.5 FPS)
        TickType_t now = xTaskGetTickCount();
        if (needs_redraw || ((now - last_oled_render) >= pdMS_TO_TICKS(80))) {
            last_oled_render = now;
            needs_redraw = false;
            Task_UI_Render(&data);
        }
    }
}

BaseType_t Task_UI_Create(void)
{
    // Single-slot queue for overwriting latest vehicle snapshot
    s_telemetry_queue = xQueueCreate(1, sizeof(VehicleData_t));
    if (s_telemetry_queue == NULL) {
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    return xTaskCreate(Task_UI_Body, "UI_Task", UI_TASK_STACK_SIZE, NULL, UI_TASK_PRIORITY, NULL);
}

QueueHandle_t Task_UI_GetTelemetryQueue(void)
{
    return s_telemetry_queue;
}
