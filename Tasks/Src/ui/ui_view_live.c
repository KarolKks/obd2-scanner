#include "ui_common.h"

// View 1: Live Sensor Telemetry (Service 01)
static int8_t s_live_scroll = 0; // Current scroll offset in live_params array

static void View_LiveList_OnEnter(void)
{
    // Reset scroll position when entering view
    s_live_scroll = 0;
}

static void View_LiveList_Render(const VehicleData_t *data)
{
    UI_RenderHeader("[ LIVE DATA (01) ]");

    // Display waiting message if no active sensor data has arrived yet
    if (data->live_params_count == 0) {
        SH1106_DrawString(14, 22, "Waiting for CAN...", &Font_6x8, SH1106_COLOR_WHITE);
        SH1106_DrawString(8, 36, "Scanning sensors...", &Font_6x8, SH1106_COLOR_WHITE);
        UI_RenderFooter("Hold: Back to menu");
        return;
    }

    // Render up to 4 visible parameter rows in current viewport
    for (uint8_t row = 0; row < 4; row++) {
        uint8_t param_idx = (uint8_t)(s_live_scroll + row);
        if (param_idx >= data->live_params_count) break;

        const OBD2_PIDDescriptor_t *desc = OBD2_GetPIDDescriptor(data->live_params[param_idx].pid);
        char line[26];
        if (desc != NULL) {
            if (data->live_params[param_idx].valid) {
                float val = data->live_params[param_idx].value;
                // Format as integer or single decimal place
                if (desc->decimals == 0) {
                    snprintf(line, sizeof(line), "%-4s: %4d %s", desc->short_name, (int)val, desc->unit);
                } else {
                    int32_t ipart = (int32_t)val;
                    int32_t fpart = (int32_t)((val - (float)ipart) * 10.0f);
                    if (fpart < 0) fpart = -fpart;
                    snprintf(line, sizeof(line), "%-4s:%3d.%01d %s", desc->short_name, (int)ipart, (int)fpart, desc->unit);
                }
            } else {
                snprintf(line, sizeof(line), "%-4s: No reply", desc->short_name);
            }
        } else {
            snprintf(line, sizeof(line), "PID %02X: %d", (unsigned int)data->live_params[param_idx].pid, (int)data->live_params[param_idx].value);
        }

        int16_t y = 12 + (row * 10);
        SH1106_DrawString(2, y, line, &Font_6x8, SH1106_COLOR_WHITE);
    }

    // Vertical scroll indicator arrows
    if (s_live_scroll > 0) {
        SH1106_DrawString(122, 12, "^", &Font_6x8, SH1106_COLOR_WHITE);
    }
    if ((s_live_scroll + 4) < data->live_params_count) {
        SH1106_DrawString(122, 42, "v", &Font_6x8, SH1106_COLOR_WHITE);
    }

    UI_RenderFooter("Rotate:Scroll Hold:Back");
}

static void View_LiveList_OnEvent(KY040_Event_t event, const VehicleData_t *data)
{
    switch (event) {
        case KY040_EVENT_CW:
            // Scroll down if more parameters exist below
            if (data->live_params_count > 4) {
                if (s_live_scroll < (data->live_params_count - 4)) s_live_scroll++;
            }
            break;
        case KY040_EVENT_CCW:
            // Scroll up towards start of list
            if (s_live_scroll > 0) s_live_scroll--;
            break;
        case KY040_EVENT_CLICK:
            // Click resets scroll to top
            s_live_scroll = 0;
            break;
        case KY040_EVENT_HOLD:
            // Long press exits back to main diagnostic menu
            UI_ExitToMenu();
            break;
        default:
            break;
    }
}

const UI_Screen_t g_screen_live = {
    .label      = "1. LIVE DATA",
    .on_enter   = View_LiveList_OnEnter,
    .render     = View_LiveList_Render,
    .on_event   = View_LiveList_OnEvent,
    .get_footer = NULL
};
