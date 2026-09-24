#ifndef __MAIN_H
#define __MAIN_H


#include "stm32l4xx.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_system.h"
#include "stm32l4xx_ll_utils.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_spi.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "clk.h"
#include "uart.h"
#include "can.h"
#include "spi.h"
#include "rtc.h"
#include "ff.h"
#include "fatfs_sd.h"
#include "obd2.h"
#include "obd_multiframe.h"
#include "logger.h"
#include "tim.h"
#include "gpio.h"
#include "ky040.h"
#include "sh1106.h"

#include "task_logger.h"
#include "task_ui.h"
#include "task_obd2.h"
#include "task_uart.h"

#endif /* __MAIN_H */
