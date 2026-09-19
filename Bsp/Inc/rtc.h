#ifndef RTC_H
#define RTC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_pwr.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_rtc.h"


/**
 * @brief RTC Driver status codes.
 */
typedef enum {
    RTC_OK          = 0,  /**< Operation completed successfully */
    RTC_ERR_INIT    = 1,  /**< Hardware initialization failure */
    RTC_ERR_PARAM   = 2,  /**< Invalid input argument */
    RTC_ERR_TIMEOUT = 3   /**< Hardware synchronization timeout */
} RTC_Status_t;

/**
 * @brief RTC Date and Time representation structure.
 */
typedef struct {
    uint16_t year;     /**< Year (e.g. 2026) */
    uint8_t  month;    /**< Month [1 - 12] */
    uint8_t  day;      /**< Day of month [1 - 31] */
    uint8_t  weekday;  /**< Day of week [1 = Mon .. 7 = Sun] */
    uint8_t  hours;    /**< Hours in 24-hour format [0 - 23] */
    uint8_t  minutes;  /**< Minutes [0 - 59] */
    uint8_t  seconds;  /**< Seconds [0 - 59] */
} RTC_DateTime_t;

/**
 * @brief  Initializes the hardware RTC peripheral using LSE (32.768 kHz) or LSI fallback.
 * @note   If the RTC was previously initialized and running on battery/power,
 *         it preserves the running calendar without re-writing.
 *         Otherwise, it sets the initial date and time parsed from build macros (__DATE__, __TIME__).
 * @return RTC_OK on success, error code otherwise.
 */
RTC_Status_t RTC_Init(void);

/**
 * @brief  Reads the current date and time from the RTC shadow registers.
 * @param[out] dt Pointer to the output date/time structure.
 * @return RTC_OK on success, error code otherwise.
 */
RTC_Status_t RTC_GetDateTime(RTC_DateTime_t *dt);

/**
 * @brief  Sets the RTC date and time.
 * @param[in] dt Pointer to the date/time structure with target values.
 * @return RTC_OK on success, error code otherwise.
 */
RTC_Status_t RTC_SetDateTime(const RTC_DateTime_t *dt);

/**
 * @brief  Formats the current RTC date and time into an ISO-like string.
 *         Format: "YYYY-MM-DD HH:MM:SS" (exactly 19 characters + null-terminator).
 * @param[out] buf     Destination buffer.
 * @param[in]  max_len Maximum buffer length (must be at least 20 bytes).
 * @return RTC_OK on success, error code otherwise.
 */
RTC_Status_t RTC_GetDateTimeString(char *buf, size_t max_len);


#endif /* RTC_H */
