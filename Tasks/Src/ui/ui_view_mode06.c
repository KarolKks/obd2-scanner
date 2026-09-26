#include "ui_common.h"

// View: On-Board Monitoring Test Results (Service 06)
static int8_t s_mode06_cursor = 0;       // Highlighted item index in list
static int8_t s_mode06_scroll = 0;       // Viewport vertical scroll offset
static bool   s_mode06_detail_view = false; // true if displaying single test details

static void View_Mode06_OnEnter(void)
{
    s_mode06_cursor = 0;
    s_mode06_scroll = 0;
    s_mode06_detail_view = false;

    // Enable periodic background Mode 06 sweep in OBD2 task
    OBD2_SetMode06Active(true);
}

static void View_Mode06_Render(const VehicleData_t *data)
{
    // If ECU or simulator timed out or sent NRC
    if (data->mode06_data.no_response) {
        UI_RenderHeader("[ ON-BOARD MON. 06 ]");
        // Centered "No response." text
        SH1106_DrawString(28, 26, "No response.", &Font_6x8, SH1106_COLOR_WHITE);
        UI_RenderFooter("Hold: Back to menu");
        return;
    }

    // Waiting for first response frame to arrive
    if (!data->mode06_data.valid || data->mode06_data.count == 0) {
        UI_RenderHeader("[ ON-BOARD MON. 06 ]");
        SH1106_DrawString(8, 22, "Reading monitors...", &Font_6x8, SH1106_COLOR_WHITE);
        SH1106_DrawString(8, 34, "Waiting for CAN...", &Font_6x8, SH1106_COLOR_WHITE);
        UI_RenderFooter("Hold: Back to menu");
        return;
    }

    // Detail view for currently selected monitor
    if (s_mode06_detail_view) {
        UI_RenderHeader("[ MONITOR DETAILS ]");

        const OBD2_Mode06Item_t *item = &data->mode06_data.items[s_mode06_cursor];
        char line[26];

        snprintf(line, sizeof(line), "MID:%02X TID:%02X", (unsigned int)item->obdmid, (unsigned int)item->tid);
        SH1106_DrawString(2, 12, line, &Font_6x8, SH1106_COLOR_WHITE);

        snprintf(line, sizeof(line), "Val:%5u %s", (unsigned int)item->value, item->passed ? "[PASS]" : "[FAIL]");
        SH1106_DrawString(2, 22, line, &Font_6x8, SH1106_COLOR_WHITE);

        if (item->min_limit == 0xFFFF) {
            snprintf(line, sizeof(line), "Min:  N/A Max:%5u", (unsigned int)item->max_limit);
        } else if (item->max_limit == 0xFFFF) {
            snprintf(line, sizeof(line), "Min:%5u Max:  N/A", (unsigned int)item->min_limit);
        } else {
            snprintf(line, sizeof(line), "Min:%5u Max:%5u", (unsigned int)item->min_limit, (unsigned int)item->max_limit);
        }
        SH1106_DrawString(2, 32, line, &Font_6x8, SH1106_COLOR_WHITE);

        snprintf(line, sizeof(line), "%.20s", item->name);
        SH1106_DrawString(2, 42, line, &Font_6x8, SH1106_COLOR_WHITE);

        UI_RenderFooter("Click/Hold: Back");
        return;
    }

    // Standard list view
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "[ ON-BOARD MON (%u) ]", (unsigned int)data->mode06_data.count);
    UI_RenderHeader(hdr);

    for (uint8_t row = 0; row < 4; row++) {
        int8_t item_idx = s_mode06_scroll + row;
        if (item_idx >= data->mode06_data.count) break;

        const OBD2_Mode06Item_t *it = &data->mode06_data.items[item_idx];
        char row_str[24];
        snprintf(row_str, sizeof(row_str), "%-13.13s %s", it->name, it->passed ? "[PASS]" : "[FAIL]");

        int16_t y = 12 + (row * 10);
        if (item_idx == s_mode06_cursor) {
            SH1106_FillRect(0, y - 1, 120, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(2, y, row_str, &Font_6x8, SH1106_COLOR_BLACK);
        } else {
            SH1106_DrawString(2, y, row_str, &Font_6x8, SH1106_COLOR_WHITE);
        }
    }

    // Scroll indicators
    if (s_mode06_scroll > 0) {
        SH1106_DrawString(122, 12, "^", &Font_6x8, SH1106_COLOR_WHITE);
    }
    if ((s_mode06_scroll + 4) < data->mode06_data.count) {
        SH1106_DrawString(122, 42, "v", &Font_6x8, SH1106_COLOR_WHITE);
    }

    UI_RenderFooter("Click:Det Hold:Back");
}

static void View_Mode06_OnEvent(KY040_Event_t event, const VehicleData_t *data)
{
    if (event == KY040_EVENT_HOLD) {
        OBD2_SetMode06Active(false);
        UI_ExitToMenu();
        return;
    }

    if (data->mode06_data.no_response) {
        if (event == KY040_EVENT_CLICK) {
            OBD2_SetMode06Active(false);
            UI_ExitToMenu();
        }
        return;
    }

    if (s_mode06_detail_view) {
        if (event == KY040_EVENT_CLICK) {
            s_mode06_detail_view = false;
        }
        return;
    }

    switch (event) {
        case KY040_EVENT_CW:
            if (data->mode06_data.count > 0 && s_mode06_cursor < (data->mode06_data.count - 1)) {
                s_mode06_cursor++;
                if (s_mode06_cursor >= (s_mode06_scroll + 4)) {
                    s_mode06_scroll = s_mode06_cursor - 3;
                }
            }
            break;
        case KY040_EVENT_CCW:
            if (s_mode06_cursor > 0) {
                s_mode06_cursor--;
                if (s_mode06_cursor < s_mode06_scroll) {
                    s_mode06_scroll = s_mode06_cursor;
                }
            }
            break;
        case KY040_EVENT_CLICK:
            if (data->mode06_data.count > 0) {
                s_mode06_detail_view = true;
            }
            break;
        default:
            break;
    }
}

const UI_Screen_t g_screen_mode06 = {
    .label      = "5. ON-BOARD MON. 06",
    .on_enter   = View_Mode06_OnEnter,
    .render     = View_Mode06_Render,
    .on_event   = View_Mode06_OnEvent,
    .get_footer = NULL
};
