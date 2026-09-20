#include "gpio.h"

GPIO_Status_t GPIO_Init(void)
{
    // Enable peripheral clock for GPIOA
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    // Configure PA10 (Arduino D2) as Push-Button input with internal Pull-Up resistor (active LOW)
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_10, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_10, LL_GPIO_PULL_UP);

    return GPIO_OK;
}

bool GPIO_Button_IsPressed(void)
{
    // Active LOW: pin reads 0 when switch is pressed (shorted to GND)
    return (LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_10) == 0);
}
