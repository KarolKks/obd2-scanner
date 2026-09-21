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

#define SD_CS_PORT  GPIOA
#define SD_CS_PIN   LL_GPIO_PIN_4

void GPIO_SD_CS_Init(void)
{
    // Enable GPIOA peripheral clock
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    // Initialize CS pin as Output Push-Pull, High Speed, Pull-Up, default HIGH (Deselected)
    LL_GPIO_SetOutputPin(SD_CS_PORT, SD_CS_PIN);
    LL_GPIO_SetPinMode(SD_CS_PORT, SD_CS_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(SD_CS_PORT, SD_CS_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(SD_CS_PORT, SD_CS_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(SD_CS_PORT, SD_CS_PIN, LL_GPIO_PULL_UP);
}

void GPIO_SD_CS_Select(void)
{
    // Active LOW: pull CS line low to select the card
    LL_GPIO_ResetOutputPin(SD_CS_PORT, SD_CS_PIN);
}

void GPIO_SD_CS_Deselect(void)
{
    // Drive CS line high to release the card
    LL_GPIO_SetOutputPin(SD_CS_PORT, SD_CS_PIN);
}

