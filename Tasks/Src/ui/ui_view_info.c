#include "ui_common.h"

// View 4: Freeze Frame Snapshot (Service 02)
static void View_FreezeFrame_Render(const VehicleData_t *data)
{
    UI_RenderHeader("[ FREEZE FRAME (02) ]");

    // Display critical engine metrics recorded when diagnostic fault was triggered
    if (data->freeze_valid) {
        char buf[28];
        snprintf(buf, sizeof(buf), "RPM    : %4d rpm", (int)data->freeze_rpm);
        SH1106_DrawString(2, 14, buf, &Font_6x8, SH1106_COLOR_WHITE);

        snprintf(buf, sizeof(buf), "Speed  : %4d km/h", (int)data->freeze_speed);
        SH1106_DrawString(2, 26, buf, &Font_6x8, SH1106_COLOR_WHITE);

        snprintf(buf, sizeof(buf), "Coolant: %4d C", (int)data->freeze_coolant);
        SH1106_DrawString(2, 38, buf, &Font_6x8, SH1106_COLOR_WHITE);
    } else {
        SH1106_DrawString(14, 20, "Freeze Frame:", &Font_6x8, SH1106_COLOR_WHITE);
        SH1106_DrawString(14, 34, "Not available in ECU", &Font_6x8, SH1106_COLOR_WHITE);
    }

    UI_RenderFooter("Hold: Back to menu");
}

static void View_FreezeFrame_OnEvent(KY040_Event_t event, const VehicleData_t *data)
{
    (void)data;
    // Any press returns to main menu
    if (event == KY040_EVENT_HOLD || event == KY040_EVENT_CLICK) {
        UI_ExitToMenu();
    }
}

const UI_Screen_t g_screen_freeze_frame = {
    .label      = "4. FREEZE FRAME",
    .on_enter   = NULL,
    .render     = View_FreezeFrame_Render,
    .on_event   = View_FreezeFrame_OnEvent,
    .get_footer = NULL
};

// View 5: Vehicle Identification (Service 09 VIN)
static void View_VehicleInfo_Render(const VehicleData_t *data)
{
    UI_RenderHeader("[ VEHICLE INFO (09) ]");

    // Display 17-character VIN decoded from multiframe ISO-TP transfer
    SH1106_DrawString(2, 14, "VIN Code:", &Font_6x8, SH1106_COLOR_WHITE);
    if (data->vin_valid) {
        SH1106_DrawString(2, 24, data->vin, &Font_6x8, SH1106_COLOR_WHITE);
    } else {
        SH1106_DrawString(2, 24, "Searching / No resp", &Font_6x8, SH1106_COLOR_WHITE);
    }

    SH1106_DrawString(2, 36, "Protocol: ISO 15765-4", &Font_6x8, SH1106_COLOR_WHITE);
    SH1106_DrawString(2, 44, "Bus: 500 kbit/s Standard", &Font_6x8, SH1106_COLOR_WHITE);

    UI_RenderFooter("Hold: Back to menu");
}

static void View_VehicleInfo_OnEvent(KY040_Event_t event, const VehicleData_t *data)
{
    (void)data;
    // Any press returns to main menu
    if (event == KY040_EVENT_HOLD || event == KY040_EVENT_CLICK) {
        UI_ExitToMenu();
    }
}

const UI_Screen_t g_screen_vehicle_info = {
    .label      = "5. VEHICLE INFO VIN",
    .on_enter   = NULL,
    .render     = View_VehicleInfo_Render,
    .on_event   = View_VehicleInfo_OnEvent,
    .get_footer = NULL
};

// View 6: Protocol & Module Information
static void View_ProtocolInfo_Render(const VehicleData_t *data)
{
    (void)data;
    UI_RenderHeader("[ PROTOCOL & MODULE ]");

    // Display hardware connection, CAN bit rate, and standard arbitration IDs
    SH1106_DrawString(2, 12, "Bus : ISO 15765-4 CAN", &Font_6x8, SH1106_COLOR_WHITE);
    SH1106_DrawString(2, 22, "Rate: 500k 11-bit ID", &Font_6x8, SH1106_COLOR_WHITE);
    SH1106_DrawString(2, 32, "Tx: 0x7DF  Rx: 0x7E8", &Font_6x8, SH1106_COLOR_WHITE);
    SH1106_DrawString(2, 42, "MCU : STM32L476RG LL", &Font_6x8, SH1106_COLOR_WHITE);

    UI_RenderFooter("Hold: Back to menu");
}

static void View_ProtocolInfo_OnEvent(KY040_Event_t event, const VehicleData_t *data)
{
    (void)data;
    // Any press returns to main menu
    if (event == KY040_EVENT_HOLD || event == KY040_EVENT_CLICK) {
        UI_ExitToMenu();
    }
}

const UI_Screen_t g_screen_protocol_info = {
    .label      = "6. PROTOCOL & MODULE",
    .on_enter   = NULL,
    .render     = View_ProtocolInfo_Render,
    .on_event   = View_ProtocolInfo_OnEvent,
    .get_footer = NULL
};
