#include "gpio.h"

GPIO_Status_t GPIO_Init(void)
{
    // Enable peripheral clock for GPIOA
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    // Configure PA10 (user push-button / encoder switch) as Input with internal Pull-Up (active LOW)
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_10, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_10, LL_GPIO_PULL_UP);

    return GPIO_OK;
}

bool GPIO_Button_IsPressed(void)
{
    // Active LOW: pin reads 0 when button contacts short to GND
    return (LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_10) == 0);
}

#define SD_CS_PORT  GPIOA
#define SD_CS_PIN   LL_GPIO_PIN_4

void GPIO_SD_CS_Init(void)
{
    // Enable GPIOA peripheral clock for SD card chip-select
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    // Configure PA4 (SD_CS) as Output Push-Pull, High Speed, default HIGH (card deselected)
    LL_GPIO_SetOutputPin(SD_CS_PORT, SD_CS_PIN);
    LL_GPIO_SetPinMode(SD_CS_PORT, SD_CS_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(SD_CS_PORT, SD_CS_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(SD_CS_PORT, SD_CS_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(SD_CS_PORT, SD_CS_PIN, LL_GPIO_PULL_UP);
}

void GPIO_SD_CS_Select(void)
{
    // Pull CS line LOW to assert active chip-select on SD card
    LL_GPIO_ResetOutputPin(SD_CS_PORT, SD_CS_PIN);
}

void GPIO_SD_CS_Deselect(void)
{
    // Drive CS line HIGH to release SD card SPI bus connection
    LL_GPIO_SetOutputPin(SD_CS_PORT, SD_CS_PIN);
}
