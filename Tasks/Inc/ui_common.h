#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "obd2.h"
#include "ky040.h"
#include "sh1106.h"
#include "font.h"
#include "rtc.h"
#include "logger.h"
#include "task_logger.h"

/**
 * @brief UI Navigation modes.
 */
typedef enum {
    UI_MODE_MENU = 0,
    UI_MODE_SUBVIEW
} UIMode_t;

/**
 * @brief Screen Descriptor Interface.
 */
typedef struct {
    const char *label;
    void        (*on_enter)(void);
    void        (*render)(const VehicleData_t *data);
    void        (*on_event)(KY040_Event_t event, const VehicleData_t *data);
    const char *(*get_footer)(void);
} UI_Screen_t;

/* --- Common UI Drawing & Navigation Functions --- */
void UI_RenderHeader(const char *title);
void UI_RenderFooter(const char *text);
void UI_ExitToMenu(void);

/* --- Registered View Descriptors --- */
// View 1: Live Sensor Telemetry (Service 01)
extern const UI_Screen_t g_screen_live;

// View 2: Diagnostic Trouble Codes (Service 03, 07, 0A)
extern const UI_Screen_t g_screen_dtc;

// View 3: Clear Fault Codes (Service 04)
extern const UI_Screen_t g_screen_clear_dtc;

// View 4: Freeze Frame Snapshot (Service 02)
extern const UI_Screen_t g_screen_freeze_frame;

// View 5: Vehicle Identification (Service 09 VIN)
extern const UI_Screen_t g_screen_vehicle_info;

// View 6: Protocol & Module Information
extern const UI_Screen_t g_screen_protocol_info;

// View 7: SD Card Logging Interval Configuration
extern const UI_Screen_t g_screen_sd_interval;

// View 8: SD Card Channel Selection
extern const UI_Screen_t g_screen_sd_channels;

// View 9: RTC Date & Time Configuration
extern const UI_Screen_t g_screen_set_datetime;

#endif /* UI_COMMON_H */
