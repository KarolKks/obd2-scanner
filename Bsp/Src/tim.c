#include "tim.h"

TIM_Status_t TIM3_Encoder_Init(void)
{
    // Enable peripheral clocks for GPIOB and TIM3
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);

    // Configure PB4 (TIM3_CH1) and PB5 (TIM3_CH2) with Alternate Function AF2 and pull-ups
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_4, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_4, LL_GPIO_AF_2);
    LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_4, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinSpeed(GPIOB, LL_GPIO_PIN_4, LL_GPIO_SPEED_FREQ_VERY_HIGH);

    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_5, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_5, LL_GPIO_AF_2);
    LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_5, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinSpeed(GPIOB, LL_GPIO_PIN_5, LL_GPIO_SPEED_FREQ_VERY_HIGH);

    // Configure TIM3 in Quadrature Encoder Interface mode (counts on TI1 transitions)
    LL_TIM_SetEncoderMode(TIM3, LL_TIM_ENCODERMODE_X2_TI1);

    // Set input capture polarity and 8-sample digital filter (N8) to eliminate mechanical switch bounce
    LL_TIM_IC_SetActiveInput(TIM3, LL_TIM_CHANNEL_CH1, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPolarity(TIM3, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_RISING);
    LL_TIM_IC_SetFilter(TIM3, LL_TIM_CHANNEL_CH1, LL_TIM_IC_FILTER_FDIV1_N8);

    LL_TIM_IC_SetActiveInput(TIM3, LL_TIM_CHANNEL_CH2, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPolarity(TIM3, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_RISING);
    LL_TIM_IC_SetFilter(TIM3, LL_TIM_CHANNEL_CH2, LL_TIM_IC_FILTER_FDIV1_N8);

    // Set 16-bit auto-reload register and clear initial count
    LL_TIM_SetAutoReload(TIM3, 0xFFFF);
    LL_TIM_SetCounter(TIM3, 0);

    // Enable hardware counter
    LL_TIM_EnableCounter(TIM3);

    return TIM_OK;
}

uint16_t TIM3_Encoder_GetCount(void)
{
    // Return current 16-bit encoder position counter
    return (uint16_t)LL_TIM_GetCounter(TIM3);
}

void TIM3_Encoder_SetCount(uint16_t val)
{
    // Overwrite hardware encoder counter value
    LL_TIM_SetCounter(TIM3, val);
}
