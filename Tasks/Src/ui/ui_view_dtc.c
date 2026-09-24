#include "ui_common.h"

// View 2: Diagnostic Trouble Codes (Service 03, 07, 0A)
static int8_t s_dtc_scroll = 0;       // Current scroll offset in flattened DTC line list
static int8_t s_dtc_total_lines = 0;  // Total rendered lines (headers + codes)

static void View_DTCs_OnEnter(void)
{
    s_dtc_scroll = 0;
}

static void View_DTCs_Render(const VehicleData_t *data)
{
    typedef struct {
        char text[26];
        bool is_header;
    } DtcLine_t;

    // Flatten all categories (stored, pending, permanent, MIL) into a linear line buffer
    DtcLine_t lines[24];
    uint8_t count = 0;

    uint8_t total_dtcs = 0;
    if (data->dtc_valid) total_dtcs += data->dtc_count;
    if (data->pending_valid) total_dtcs += data->pending_count;
    if (data->permanent_valid) total_dtcs += data->permanent_count;

    char hdr[32];
    snprintf(hdr, sizeof(hdr), "[ TROUBLE CODES (%u) ]", (unsigned int)total_dtcs);
    UI_RenderHeader(hdr);

    // Mode 03 Stored DTCs (confirmed fault codes)
    snprintf(lines[count].text, sizeof(lines[count].text), "03 STORED CODES (%u)",
             data->dtc_valid ? (unsigned int)data->dtc_count : 0);
    lines[count].is_header = true;
    count++;

    if (!data->dtc_valid) {
        snprintf(lines[count].text, sizeof(lines[count].text), "  No response");
        lines[count].is_header = false;
        count++;
    } else if (data->dtc_count == 0) {
        snprintf(lines[count].text, sizeof(lines[count].text), "  No DTCs in ECU");
        lines[count].is_header = false;
        count++;
    } else {
        for (uint8_t i = 0; i < data->dtc_count && i < 6 && count < 24; i++) {
            char code_str[8];
            OBD2_FormatDTC(data->dtc_codes[i], code_str);
            const char *desc = OBD2_GetDTCDescription(data->dtc_codes[i]);
            snprintf(lines[count].text, sizeof(lines[count].text), "%u. %-5s %.11s",
                     (unsigned int)(i + 1), code_str, desc);
            lines[count].is_header = false;
            count++;
        }
    }

    // Mode 07 Pending DTCs (faults detected during current drive cycle)
    snprintf(lines[count].text, sizeof(lines[count].text), "07 PENDING CODES (%u)",
             data->pending_valid ? (unsigned int)data->pending_count : 0);
    lines[count].is_header = true;
    count++;

    if (!data->pending_valid) {
        snprintf(lines[count].text, sizeof(lines[count].text), "  No response");
        lines[count].is_header = false;
        count++;
    } else if (data->pending_count == 0) {
        snprintf(lines[count].text, sizeof(lines[count].text), "  No pending codes");
        lines[count].is_header = false;
        count++;
    } else {
        for (uint8_t i = 0; i < data->pending_count && i < 6 && count < 24; i++) {
            char code_str[8];
            OBD2_FormatDTC(data->pending_codes[i], code_str);
            const char *desc = OBD2_GetDTCDescription(data->pending_codes[i]);
            snprintf(lines[count].text, sizeof(lines[count].text), "%u. %-5s %.11s",
                     (unsigned int)(i + 1), code_str, desc);
            lines[count].is_header = false;
            count++;
        }
    }

    // Mode 0A Permanent DTCs (faults cleared only when self-test passes)
    snprintf(lines[count].text, sizeof(lines[count].text), "0A PERMANENT (%u)",
             data->permanent_valid ? (unsigned int)data->permanent_count : 0);
    lines[count].is_header = true;
    count++;

    if (!data->permanent_valid) {
        snprintf(lines[count].text, sizeof(lines[count].text), "  No response");
        lines[count].is_header = false;
        count++;
    } else if (data->permanent_count == 0) {
        snprintf(lines[count].text, sizeof(lines[count].text), "  No perm codes");
        lines[count].is_header = false;
        count++;
    } else {
        for (uint8_t i = 0; i < data->permanent_count && i < 6 && count < 24; i++) {
            char code_str[8];
            OBD2_FormatDTC(data->permanent_codes[i], code_str);
            const char *desc = OBD2_GetDTCDescription(data->permanent_codes[i]);
            snprintf(lines[count].text, sizeof(lines[count].text), "%u. %-5s %.11s",
                     (unsigned int)(i + 1), code_str, desc);
            lines[count].is_header = false;
            count++;
        }
    }

    // MIL (Check Engine) lamp status line
    snprintf(lines[count].text, sizeof(lines[count].text), "MIL Status : [%s]",
             (total_dtcs > 0) ? " ON  " : " OFF ");
    lines[count].is_header = false;
    count++;

    s_dtc_total_lines = count;

    // Clamp scroll offset to valid visible range
    if (s_dtc_total_lines > 4) {
        if (s_dtc_scroll > (s_dtc_total_lines - 4)) {
            s_dtc_scroll = s_dtc_total_lines - 4;
        }
    } else {
        s_dtc_scroll = 0;
    }
    if (s_dtc_scroll < 0) s_dtc_scroll = 0;

    // Render 4 lines (category headers highlighted with inverted colors)
    for (uint8_t r = 0; r < 4; r++) {
        int idx = s_dtc_scroll + r;
        if (idx >= count) break;

        int16_t y = 12 + (r * 10);
        if (lines[idx].is_header) {
            SH1106_FillRect(0, y - 1, 120, 9, SH1106_COLOR_WHITE);
            SH1106_DrawString(2, y, lines[idx].text, &Font_6x8, SH1106_COLOR_BLACK);
        } else {
            SH1106_DrawString(2, y, lines[idx].text, &Font_6x8, SH1106_COLOR_WHITE);
        }
    }

    // Scroll indicators
    if (s_dtc_scroll > 0) {
        SH1106_DrawString(122, 12, "^", &Font_6x8, SH1106_COLOR_WHITE);
    }
    if ((s_dtc_scroll + 4) < count) {
        SH1106_DrawString(122, 42, "v", &Font_6x8, SH1106_COLOR_WHITE);
    }

    UI_RenderFooter("Rotate:Scroll Hold:Back");
}

static void View_DTCs_OnEvent(KY040_Event_t event, const VehicleData_t *data)
{
    (void)data;
    switch (event) {
        case KY040_EVENT_CW:
            // Scroll down through trouble codes
            if (s_dtc_total_lines > 4) {
                if (s_dtc_scroll < (s_dtc_total_lines - 4)) s_dtc_scroll++;
            }
            break;
        case KY040_EVENT_CCW:
            // Scroll up through trouble codes
            if (s_dtc_scroll > 0) s_dtc_scroll--;
            break;
        case KY040_EVENT_CLICK:
            // Reset to top of list
            s_dtc_scroll = 0;
            break;
        case KY040_EVENT_HOLD:
            // Return to main menu
            UI_ExitToMenu();
            break;
        default:
            break;
    }
}

const UI_Screen_t g_screen_dtc = {
    .label      = "2. TROUBLE CODES DTC",
    .on_enter   = View_DTCs_OnEnter,
    .render     = View_DTCs_Render,
    .on_event   = View_DTCs_OnEvent,
    .get_footer = NULL
};

// View 3: Clear Fault Codes (Service 04)
static int8_t s_clear_confirm_choice = 1; // 0 = YES, 1 = NO (default to safe option)

static void View_ClearDTC_OnEnter(void)
{
    s_clear_confirm_choice = 1;
    OBD2_SetClearDTCStatus(0);
}

static void View_ClearDTC_Render(const VehicleData_t *data)
{
    (void)data;
    UI_RenderHeader("[ CLEAR FAULT CODES ]");

    int8_t status = OBD2_GetClearDTCStatus();

    SH1106_DrawString(2, 12, "Clear codes & MIL?", &Font_6x8, SH1106_COLOR_WHITE);

    // Confirmation buttons (highlight active selection)
    if (s_clear_confirm_choice == 0) {
        SH1106_FillRect(10, 24, 40, 10, SH1106_COLOR_WHITE);
        SH1106_DrawString(16, 25, "YES", &Font_6x8, SH1106_COLOR_BLACK);
        SH1106_DrawRect(70, 24, 40, 10, SH1106_COLOR_WHITE);
        SH1106_DrawString(76, 25, "NO", &Font_6x8, SH1106_COLOR_WHITE);
    } else {
        SH1106_DrawRect(10, 24, 40, 10, SH1106_COLOR_WHITE);
        SH1106_DrawString(16, 25, "YES", &Font_6x8, SH1106_COLOR_WHITE);
        SH1106_FillRect(70, 24, 40, 10, SH1106_COLOR_WHITE);
        SH1106_DrawString(76, 25, "NO", &Font_6x8, SH1106_COLOR_BLACK);
    }

    // Display execution feedback status from OBD2 task
    if (status == 1) {
        SH1106_DrawString(2, 40, "Status: Clearing...", &Font_6x8, SH1106_COLOR_WHITE);
    } else if (status == 2) {
        SH1106_DrawString(2, 40, "Status: CODES CLEARED", &Font_6x8, SH1106_COLOR_WHITE);
    } else if (status == 3) {
        SH1106_DrawString(2, 40, "Status: NO RESPONSE", &Font_6x8, SH1106_COLOR_WHITE);
    } else {
        SH1106_DrawString(2, 40, "Press YES to confirm", &Font_6x8, SH1106_COLOR_WHITE);
    }

    UI_RenderFooter("Rotate:Select Click:Exec");
}

static void View_ClearDTC_OnEvent(KY040_Event_t event, const VehicleData_t *data)
{
    (void)data;
    switch (event) {
        case KY040_EVENT_CW:
        case KY040_EVENT_CCW:
            // Toggle between YES and NO
            s_clear_confirm_choice = (s_clear_confirm_choice == 0) ? 1 : 0;
            break;
        case KY040_EVENT_CLICK:
            if (s_clear_confirm_choice == 0) {
                // Request Service 04 Clear DTC from OBD-II core task
                OBD2_TriggerClearDTC();
            } else {
                UI_ExitToMenu();
            }
            break;
        case KY040_EVENT_HOLD:
            UI_ExitToMenu();
            break;
        default:
            break;
    }
}

const UI_Screen_t g_screen_clear_dtc = {
    .label      = "3. CLEAR CODES",
    .on_enter   = View_ClearDTC_OnEnter,
    .render     = View_ClearDTC_Render,
    .on_event   = View_ClearDTC_OnEvent,
    .get_footer = NULL
};
