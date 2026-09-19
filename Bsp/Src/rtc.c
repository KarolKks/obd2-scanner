#include "rtc.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_pwr.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_rtc.h"
#include <stdio.h>
#include <string.h>

#define RTC_TIMEOUT_LOOPS       1000000U
#define RTC_CENTURY             2000U

/**
 * @brief Parses compiler __DATE__ ("Mmm dd yyyy") and __TIME__ ("hh:mm:ss") into RTC structure.
 */
static void RTC_ParseBuildDateTime(RTC_DateTime_t *dt)
{
    const char *date_str = __DATE__;
    const char *time_str = __TIME__;
    const char *months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    // Default fallback date: 2026-01-01 00:00:00
    dt->year    = 2026;
    dt->month   = 1;
    dt->day     = 1;
    dt->weekday = 1;
    dt->hours   = 0;
    dt->minutes = 0;
    dt->seconds = 0;

    // Parse Month
    for (uint8_t i = 0; i < 12; i++) {
        if (strncmp(date_str, months[i], 3) == 0) {
            dt->month = i + 1;
            break;
        }
    }

    // Parse Day and Year
    int day_val = 1;
    int year_val = 2026;
    sscanf(date_str + 4, "%d %d", &day_val, &year_val);
    dt->day  = (uint8_t)day_val;
    dt->year = (uint16_t)year_val;

    // Parse Time: "hh:mm:ss"
    int h = 0, m = 0, s = 0;
    sscanf(time_str, "%d:%d:%d", &h, &m, &s);
    dt->hours   = (uint8_t)h;
    dt->minutes = (uint8_t)m;
    dt->seconds = (uint8_t)s;
}

RTC_Status_t RTC_Init(void)
{
    // Enable PWR clock and unlock access to the backup domain
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
    LL_PWR_EnableBkUpAccess();

    // Check if RTC is already configured and calendar running (e.g. preserved by VBAT)
    if (LL_RCC_IsEnabledRTC() && LL_RTC_IsActiveFlag_INITS(RTC)) {
        return RTC_OK;
    }

    // Try starting LSE (32.768 kHz external crystal on Nucleo board)
    LL_RCC_LSE_Enable();
    uint32_t timeout = RTC_TIMEOUT_LOOPS;
    while (!LL_RCC_LSE_IsReady()) {
        if (--timeout == 0) {
            break; // LSE timed out, fall back to LSI
        }
    }

    LL_RTC_InitTypeDef rtc_init;
    rtc_init.HourFormat = LL_RTC_HOURFORMAT_24HOUR;

    if (LL_RCC_LSE_IsReady()) {
        // LSE successful: 32.768 kHz -> Asynch=127, Synch=255
        LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSE);
        rtc_init.AsynchPrescaler = 127U;
        rtc_init.SynchPrescaler  = 255U;
    } else {
        // Fallback to internal LSI oscillator (~32 kHz)
        LL_RCC_LSI_Enable();
        timeout = RTC_TIMEOUT_LOOPS;
        while (!LL_RCC_LSI_IsReady()) {
            if (--timeout == 0) {
                return RTC_ERR_INIT;
            }
        }
        LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSI);
        rtc_init.AsynchPrescaler = 127U;
        rtc_init.SynchPrescaler  = 249U; // 32000 / (128 * 250) = 1 Hz
    }

    // Enable RTC peripheral clock
    LL_RCC_EnableRTC();

    // Initialize RTC prescalers and 24-hour format
    if (LL_RTC_Init(RTC, &rtc_init) != SUCCESS) {
        return RTC_ERR_INIT;
    }

    // Initialize default time and date from compilation timestamp
    RTC_DateTime_t initial_dt;
    RTC_ParseBuildDateTime(&initial_dt);

    if (RTC_SetDateTime(&initial_dt) != RTC_OK) {
        return RTC_ERR_INIT;
    }

    return RTC_OK;
}

RTC_Status_t RTC_SetDateTime(const RTC_DateTime_t *dt)
{
    if (dt == NULL) {
        return RTC_ERR_PARAM;
    }

    LL_RTC_TimeTypeDef time_struct;
    time_struct.TimeFormat = LL_RTC_TIME_FORMAT_AM_OR_24;
    time_struct.Hours      = dt->hours;
    time_struct.Minutes    = dt->minutes;
    time_struct.Seconds    = dt->seconds;

    if (LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BIN, &time_struct) != SUCCESS) {
        return RTC_ERR_INIT;
    }

    LL_RTC_DateTypeDef date_struct;
    date_struct.Year    = (uint8_t)(dt->year >= RTC_CENTURY ? (dt->year - RTC_CENTURY) : dt->year);
    date_struct.Month   = dt->month;
    date_struct.Day     = dt->day;
    date_struct.WeekDay = dt->weekday;

    if (LL_RTC_DATE_Init(RTC, LL_RTC_FORMAT_BIN, &date_struct) != SUCCESS) {
        return RTC_ERR_INIT;
    }

    return RTC_OK;
}

RTC_Status_t RTC_GetDateTime(RTC_DateTime_t *dt)
{
    if (dt == NULL) {
        return RTC_ERR_PARAM;
    }

    // Must read Time register first to freeze shadow registers, then Date register
    dt->hours   = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetHour(RTC));
    dt->minutes = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetMinute(RTC));
    dt->seconds = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetSecond(RTC));

    dt->year    = (uint16_t)(RTC_CENTURY + __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetYear(RTC)));
    dt->month   = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetMonth(RTC));
    dt->day     = (uint8_t)__LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetDay(RTC));
    dt->weekday = (uint8_t)LL_RTC_DATE_GetWeekDay(RTC);

    return RTC_OK;
}

RTC_Status_t RTC_GetDateTimeString(char *buf, size_t max_len)
{
    if (buf == NULL || max_len < 20U) {
        return RTC_ERR_PARAM;
    }

    RTC_DateTime_t dt;
    RTC_Status_t status = RTC_GetDateTime(&dt);
    if (status != RTC_OK) {
        return status;
    }

    // Format: "YYYY-MM-DD HH:MM:SS"
    snprintf(buf, max_len, "%04u-%02u-%02u %02u:%02u:%02u",
             (unsigned int)dt.year,
             (unsigned int)dt.month,
             (unsigned int)dt.day,
             (unsigned int)dt.hours,
             (unsigned int)dt.minutes,
             (unsigned int)dt.seconds);

    return RTC_OK;
}
