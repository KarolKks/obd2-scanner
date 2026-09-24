#include "ui_common.h"

// View 7: SD Card Logging Interval Configuration
static uint16_t s_edit_interval_sec = 0; // 0 = disabled/paused, 1..60 = interval in seconds
static char s_sd_footer_buf[32];

static void View_SDInterval_OnEnter(void)
{
    s_edit_interval_sec = Task_Logger_GetIntervalSeconds();
}

static const char *View_SDInterval_GetFooter(void)
{
    uint16_t act = Task_Logger_GetIntervalSeconds();
    if (act == 0) {
        snprintf(s_sd_footer_buf, sizeof(s_sd_footer_buf), "SD: DISABLED Click:OK");
    } else {
        snprintf(s_sd_footer_buf, sizeof(s_sd_footer_buf), "SD: every %us Click:OK", (unsigned int)act);
    }
    return s_sd_footer_buf;
}

static void View_SDInterval_Render(const VehicleData_t *data)
{
    (void)data;
    UI_RenderHeader("[ SD LOG INTERVAL ]");

    char buf[28];
    uint16_t current_active = Task_Logger_GetIntervalSeconds();
    if (current_active == 0) {
        snprintf(buf, sizeof(buf), "Status: [STOP / PAUSED]");
    } else {
        snprintf(buf, sizeof(buf), "Status: [LOG every %us]", (unsigned int)current_active);
    }
    SH1106_DrawString(2, 12, buf, &Font_6x8, SH1106_COLOR_WHITE);

    // Box showing current selected interval
    SH1106_DrawRect(14, 23, 100, 18, SH1106_COLOR_WHITE);

    if (s_edit_interval_sec == 0) {
        SH1106_DrawString(26, 28, "< [ OFF ] >", &Font_6x8, SH1106_COLOR_WHITE);
    } else {
        snprintf(buf, sizeof(buf), "<  [ %02u ] sec  >", (unsigned int)s_edit_interval_sec);
        SH1106_DrawString(20, 28, buf, &Font_6x8, SH1106_COLOR_WHITE);
    }

    // Hardware status: card mounted or missing
    if (Logger_IsReady()) {
        SH1106_DrawString(6, 44, "SD Card: [READY]", &Font_6x8, SH1106_COLOR_WHITE);
    } else {
        SH1106_DrawString(6, 44, "SD Card: [NO CARD!]", &Font_6x8, SH1106_COLOR_WHITE);
    }

    UI_RenderFooter("Rotate:+/- Click:Confirm");
}

static void View_SDInterval_OnEvent(KY040_Event_t event, const VehicleData_t *data)
{
    (void)data;
    switch (event) {
        case KY040_EVENT_CW:
            // Increment interval up to 60s
            if (s_edit_interval_sec < 60) {
                s_edit_interval_sec++;
                Task_Logger_SetIntervalSeconds(s_edit_interval_sec);
            }
            break;
        case KY040_EVENT_CCW:
            // Decrement interval down to 0 (OFF)
            if (s_edit_interval_sec > 0) {
                s_edit_interval_sec--;
                Task_Logger_SetIntervalSeconds(s_edit_interval_sec);
            }
            break;
        case KY040_EVENT_CLICK:
            // Confirm and commit interval setting
            Task_Logger_SetIntervalSeconds(s_edit_interval_sec);
            UI_ExitToMenu();
            break;
        case KY040_EVENT_HOLD:
            UI_ExitToMenu();
            break;
        default:
            break;
    }
}

const UI_Screen_t g_screen_sd_interval = {
    .label      = "7. SD LOG INTERVAL",
    .on_enter   = View_SDInterval_OnEnter,
    .render     = View_SDInterval_Render,
    .on_event   = View_SDInterval_OnEvent,
    .get_footer = View_SDInterval_GetFooter
};

// View 8: SD Card Channel Selection
static int8_t s_channel_cursor = 0;  // 0 = ALL CHANNELS toggle, 1..20 = individual PID channels
static int8_t s_channel_scroll = 0;  // Viewport scroll offset
static char s_chan_footer_buf[32];

static void View_SDChannels_OnEnter(void)
{
    s_channel_cursor = 0;
    s_channel_scroll = 0;
}

static const char *View_SDChannels_GetFooter(void)
{
    snprintf(s_chan_footer_buf, sizeof(s_chan_footer_buf), "Channels: %02u/20 Click",
             (unsigned int)Task_Logger_GetSelectedCount());
    return s_chan_footer_buf;
}

static void View_SDChannels_Render(const VehicleData_t *data)
{
    (void)data;
    char buf[32];
    uint8_t sel_count = Task_Logger_GetSelectedCount();
    snprintf(buf, sizeof(buf), "[ CHANNELS (%02u/20) ]", (unsigned int)sel_count);
    UI_RenderHeader(buf);

    // 4-row viewport rendering
    for (uint8_t row = 0; row < 4; row++) {
        int8_t item_idx = s_channel_scroll + row;
        if (item_idx > 20) break;

        int16_t y = 12 + (row * 10);
        char row_str[24];

        if (item_idx == 0) {
            // Master toggle for all 20 logging channels
            bool all_sel = (sel_count == 20);
            snprintf(row_str, sizeof(row_str), "%s ALL CHANNELS", all_sel ? "[*]" : "[ ]");
        } else {
            // Individual sensor channel toggle
            uint8_t ch = (uint8_t)(item_idx - 1);
            const OBD2_PIDDescriptor_t *desc = OBD2_GetDescriptorByIndex(ch);
            bool is_sel = Task_Logger_IsChannelEnabled(ch);
            if (desc != NULL) {
                snprintf(row_str, sizeof(row_str), "%s %-4s [%s]",
                         is_sel ? "[*]" : "[ ]", desc->short_name, desc->unit);
            } else {
                snprintf(row_str, sizeof(row_str), "%s CH %02u", is_sel ? "[*]" : "[ ]", ch + 1);
            }
        }

        // Highlight currently focused channel row
        if (item_idx == s_channel_cursor) {
            SH1106_FillRect(0, y - 1, 120, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(2, y, row_str, &Font_6x8, SH1106_COLOR_BLACK);
        } else {
            SH1106_DrawString(2, y, row_str, &Font_6x8, SH1106_COLOR_WHITE);
        }
    }

    // Scroll indicators
    if (s_channel_scroll > 0) {
        SH1106_DrawString(122, 12, "^", &Font_6x8, SH1106_COLOR_WHITE);
    }
    if ((s_channel_scroll + 4) <= 20) {
        SH1106_DrawString(122, 42, "v", &Font_6x8, SH1106_COLOR_WHITE);
    }

    UI_RenderFooter("Rotate:Move Click:Toggle");
}

static void View_SDChannels_OnEvent(KY040_Event_t event, const VehicleData_t *data)
{
    (void)data;
    switch (event) {
        case KY040_EVENT_CW:
            // Move cursor down and scroll viewport if needed
            if (s_channel_cursor < 20) {
                s_channel_cursor++;
                if (s_channel_cursor >= (s_channel_scroll + 4)) {
                    s_channel_scroll = s_channel_cursor - 3;
                }
            }
            break;
        case KY040_EVENT_CCW:
            // Move cursor up and scroll viewport if needed
            if (s_channel_cursor > 0) {
                s_channel_cursor--;
                if (s_channel_cursor < s_channel_scroll) {
                    s_channel_scroll = s_channel_cursor;
                }
            }
            break;
        case KY040_EVENT_CLICK:
            // Click toggles all channels (row 0) or single channel (rows 1..20)
            if (s_channel_cursor == 0) {
                bool all_sel = (Task_Logger_GetSelectedCount() == 20);
                Task_Logger_SelectAllChannels(!all_sel);
            } else {
                Task_Logger_ToggleChannel((uint8_t)(s_channel_cursor - 1));
            }
            break;
        case KY040_EVENT_HOLD:
            UI_ExitToMenu();
            break;
        default:
            break;
    }
}

const UI_Screen_t g_screen_sd_channels = {
    .label      = "8. SD LOG CHANNELS",
    .on_enter   = View_SDChannels_OnEnter,
    .render     = View_SDChannels_Render,
    .on_event   = View_SDChannels_OnEvent,
    .get_footer = View_SDChannels_GetFooter
};

// View 9: RTC Date & Time Configuration
typedef enum {
    TIME_FIELD_YEAR = 0,
    TIME_FIELD_MONTH,
    TIME_FIELD_DAY,
    TIME_FIELD_HOUR,
    TIME_FIELD_MIN,
    TIME_FIELD_SAVE,
    TIME_FIELD_CANCEL,
    TIME_FIELD_COUNT
} TimeField_t;

static int8_t s_time_field = 0;          // Currently selected field in editor
static bool s_time_edit_active = false;  // true if adjusting value, false if navigating fields
static RTC_DateTime_t s_edit_dt;         // Staging buffer before committing to hardware RTC
static char s_time_footer_buf[32];

// Days in month calculation with leap year support
static uint8_t UI_GetDaysInMonth(uint16_t year, uint8_t month)
{
    if (month == 2) {
        bool leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
        return leap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }
    return 31;
}

static void View_SetDateTime_OnEnter(void)
{
    // Load current RTC time into staging structure
    if (RTC_GetDateTime(&s_edit_dt) != RTC_OK) {
        s_edit_dt.year = 2026;
        s_edit_dt.month = 1;
        s_edit_dt.day = 1;
        s_edit_dt.hours = 12;
        s_edit_dt.minutes = 0;
        s_edit_dt.seconds = 0;
    }
    s_time_field = 0;
    s_time_edit_active = false;
}

static const char *View_SetDateTime_GetFooter(void)
{
    char dt_str[24];
    if (RTC_GetDateTimeString(dt_str, sizeof(dt_str)) == RTC_OK) {
        snprintf(s_time_footer_buf, sizeof(s_time_footer_buf), "%s", dt_str);
    } else {
        snprintf(s_time_footer_buf, sizeof(s_time_footer_buf), "Set RTC Date & Time");
    }
    return s_time_footer_buf;
}

static void View_SetDateTime_Render(const VehicleData_t *data)
{
    (void)data;
    UI_RenderHeader("[ SET DATE & TIME ]");

    char buf[16];

    // Date row: DATE: YYYY-MM-DD
    SH1106_DrawString(2, 15, "DATE:", &Font_6x8, SH1106_COLOR_WHITE);

    // Year field
    snprintf(buf, sizeof(buf), "%04u", (unsigned int)s_edit_dt.year);
    if (s_time_field == TIME_FIELD_YEAR) {
        if (s_time_edit_active) {
            SH1106_FillRect(35, 14, 26, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(36, 15, buf, &Font_6x8, SH1106_COLOR_BLACK);
        } else {
            SH1106_DrawRect(35, 14, 26, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(36, 15, buf, &Font_6x8, SH1106_COLOR_WHITE);
        }
    } else {
        SH1106_DrawString(36, 15, buf, &Font_6x8, SH1106_COLOR_WHITE);
    }

    SH1106_DrawString(63, 15, "-", &Font_6x8, SH1106_COLOR_WHITE);

    // Month field
    snprintf(buf, sizeof(buf), "%02u", (unsigned int)s_edit_dt.month);
    if (s_time_field == TIME_FIELD_MONTH) {
        if (s_time_edit_active) {
            SH1106_FillRect(70, 14, 14, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(71, 15, buf, &Font_6x8, SH1106_COLOR_BLACK);
        } else {
            SH1106_DrawRect(70, 14, 14, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(71, 15, buf, &Font_6x8, SH1106_COLOR_WHITE);
        }
    } else {
        SH1106_DrawString(71, 15, buf, &Font_6x8, SH1106_COLOR_WHITE);
    }

    SH1106_DrawString(86, 15, "-", &Font_6x8, SH1106_COLOR_WHITE);

    // Day field
    snprintf(buf, sizeof(buf), "%02u", (unsigned int)s_edit_dt.day);
    if (s_time_field == TIME_FIELD_DAY) {
        if (s_time_edit_active) {
            SH1106_FillRect(93, 14, 14, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(94, 15, buf, &Font_6x8, SH1106_COLOR_BLACK);
        } else {
            SH1106_DrawRect(93, 14, 14, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(94, 15, buf, &Font_6x8, SH1106_COLOR_WHITE);
        }
    } else {
        SH1106_DrawString(94, 15, buf, &Font_6x8, SH1106_COLOR_WHITE);
    }

    // Time row: TIME: HH:MM:00
    SH1106_DrawString(2, 29, "TIME:", &Font_6x8, SH1106_COLOR_WHITE);

    // Hours field
    snprintf(buf, sizeof(buf), "%02u", (unsigned int)s_edit_dt.hours);
    if (s_time_field == TIME_FIELD_HOUR) {
        if (s_time_edit_active) {
            SH1106_FillRect(35, 28, 14, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(36, 29, buf, &Font_6x8, SH1106_COLOR_BLACK);
        } else {
            SH1106_DrawRect(35, 28, 14, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(36, 29, buf, &Font_6x8, SH1106_COLOR_WHITE);
        }
    } else {
        SH1106_DrawString(36, 29, buf, &Font_6x8, SH1106_COLOR_WHITE);
    }

    SH1106_DrawString(51, 29, ":", &Font_6x8, SH1106_COLOR_WHITE);

    // Minutes field
    snprintf(buf, sizeof(buf), "%02u", (unsigned int)s_edit_dt.minutes);
    if (s_time_field == TIME_FIELD_MIN) {
        if (s_time_edit_active) {
            SH1106_FillRect(58, 28, 14, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(59, 29, buf, &Font_6x8, SH1106_COLOR_BLACK);
        } else {
            SH1106_DrawRect(58, 28, 14, 10, SH1106_COLOR_WHITE);
            SH1106_DrawString(59, 29, buf, &Font_6x8, SH1106_COLOR_WHITE);
        }
    } else {
        SH1106_DrawString(59, 29, buf, &Font_6x8, SH1106_COLOR_WHITE);
    }

    SH1106_DrawString(74, 29, ":00", &Font_6x8, SH1106_COLOR_WHITE);

    // Action buttons: SAVE & CANCEL
    if (s_time_field == TIME_FIELD_SAVE) {
        SH1106_FillRect(8, 41, 48, 11, SH1106_COLOR_WHITE);
        SH1106_DrawString(18, 43, "SAVE", &Font_6x8, SH1106_COLOR_BLACK);
    } else {
        SH1106_DrawRect(8, 41, 48, 11, SH1106_COLOR_WHITE);
        SH1106_DrawString(18, 43, "SAVE", &Font_6x8, SH1106_COLOR_WHITE);
    }

    if (s_time_field == TIME_FIELD_CANCEL) {
        SH1106_FillRect(66, 41, 54, 11, SH1106_COLOR_WHITE);
        SH1106_DrawString(73, 43, "CANCEL", &Font_6x8, SH1106_COLOR_BLACK);
    } else {
        SH1106_DrawRect(66, 41, 54, 11, SH1106_COLOR_WHITE);
        SH1106_DrawString(73, 43, "CANCEL", &Font_6x8, SH1106_COLOR_WHITE);
    }

    if (s_time_edit_active) {
        UI_RenderFooter("Rotate:Adj  Click:Set");
    } else {
        UI_RenderFooter("Rotate:Nav  Click:Sel");
    }
}

static void View_SetDateTime_OnEvent(KY040_Event_t event, const VehicleData_t *data)
{
    (void)data;
    switch (event) {
        case KY040_EVENT_CW:
            if (!s_time_edit_active) {
                // Navigate forward to next field
                if (s_time_field < (TIME_FIELD_COUNT - 1)) s_time_field++;
            } else {
                // Increment active field value with rollover and day clamping
                if (s_time_field == TIME_FIELD_YEAR) {
                    if (s_edit_dt.year < 2099) s_edit_dt.year++;
                    uint8_t md = UI_GetDaysInMonth(s_edit_dt.year, s_edit_dt.month);
                    if (s_edit_dt.day > md) s_edit_dt.day = md;
                } else if (s_time_field == TIME_FIELD_MONTH) {
                    if (s_edit_dt.month < 12) s_edit_dt.month++; else s_edit_dt.month = 1;
                    uint8_t md = UI_GetDaysInMonth(s_edit_dt.year, s_edit_dt.month);
                    if (s_edit_dt.day > md) s_edit_dt.day = md;
                } else if (s_time_field == TIME_FIELD_DAY) {
                    uint8_t md = UI_GetDaysInMonth(s_edit_dt.year, s_edit_dt.month);
                    if (s_edit_dt.day < md) s_edit_dt.day++; else s_edit_dt.day = 1;
                } else if (s_time_field == TIME_FIELD_HOUR) {
                    if (s_edit_dt.hours < 23) s_edit_dt.hours++; else s_edit_dt.hours = 0;
                } else if (s_time_field == TIME_FIELD_MIN) {
                    if (s_edit_dt.minutes < 59) s_edit_dt.minutes++; else s_edit_dt.minutes = 0;
                }
            }
            break;

        case KY040_EVENT_CCW:
            if (!s_time_edit_active) {
                // Navigate backward to previous field
                if (s_time_field > 0) s_time_field--;
            } else {
                // Decrement active field value with rollover and day clamping
                if (s_time_field == TIME_FIELD_YEAR) {
                    if (s_edit_dt.year > 2024) s_edit_dt.year--;
                    uint8_t md = UI_GetDaysInMonth(s_edit_dt.year, s_edit_dt.month);
                    if (s_edit_dt.day > md) s_edit_dt.day = md;
                } else if (s_time_field == TIME_FIELD_MONTH) {
                    if (s_edit_dt.month > 1) s_edit_dt.month--; else s_edit_dt.month = 12;
                    uint8_t md = UI_GetDaysInMonth(s_edit_dt.year, s_edit_dt.month);
                    if (s_edit_dt.day > md) s_edit_dt.day = md;
                } else if (s_time_field == TIME_FIELD_DAY) {
                    uint8_t md = UI_GetDaysInMonth(s_edit_dt.year, s_edit_dt.month);
                    if (s_edit_dt.day > 1) s_edit_dt.day--; else s_edit_dt.day = md;
                } else if (s_time_field == TIME_FIELD_HOUR) {
                    if (s_edit_dt.hours > 0) s_edit_dt.hours--; else s_edit_dt.hours = 23;
                } else if (s_time_field == TIME_FIELD_MIN) {
                    if (s_edit_dt.minutes > 0) s_edit_dt.minutes--; else s_edit_dt.minutes = 59;
                }
            }
            break;

        case KY040_EVENT_CLICK:
            if (s_time_field <= TIME_FIELD_MIN) {
                // Toggle edit mode for selected field
                s_time_edit_active = !s_time_edit_active;
            } else if (s_time_field == TIME_FIELD_SAVE) {
                // Commit new date/time to hardware RTC
                s_edit_dt.seconds = 0;
                RTC_SetDateTime(&s_edit_dt);
                UI_ExitToMenu();
            } else if (s_time_field == TIME_FIELD_CANCEL) {
                UI_ExitToMenu();
            }
            break;

        case KY040_EVENT_HOLD:
            if (s_time_edit_active) {
                s_time_edit_active = false;
            } else {
                UI_ExitToMenu();
            }
            break;

        default:
            break;
    }
}

const UI_Screen_t g_screen_set_datetime = {
    .label      = "9. SET DATE & TIME",
    .on_enter   = View_SetDateTime_OnEnter,
    .render     = View_SetDateTime_Render,
    .on_event   = View_SetDateTime_OnEvent,
    .get_footer = View_SetDateTime_GetFooter
};
