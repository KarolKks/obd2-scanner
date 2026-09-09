#include "bsp_clk.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_system.h"
#include "stm32l4xx_ll_pwr.h"
#include "stm32l4xx_ll_utils.h"

BSP_CLK_Status_t BSP_CLK_Init(void)
{
    // Enable power interface clock and set voltage regulator to Range 1 (required for 80 MHz)
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

    // Enable internal HSI16 oscillator
    LL_RCC_HSI_Enable();
    uint32_t timeout = 100000;
    while (LL_RCC_HSI_IsReady() != 1) {
        if (--timeout == 0) {
            return BSP_CLK_ERR_INIT;
        }
    }

    // Configure Flash memory latency (4 wait states for 80 MHz) and enable acceleration buffers
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_4) {}
    LL_FLASH_EnablePrefetch();
    LL_FLASH_EnableInstCache();
    LL_FLASH_EnableDataCache();

    // Configure main PLL:
    // Source: HSI16 (16 MHz)
    // PLLM = DIV_1 -> 16 MHz VCO input
    // PLLN = 10    -> 160 MHz VCO output
    // PLLR = DIV_2 -> 80 MHz SYSCLK
    LL_RCC_PLL_Disable();
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_1, 10, LL_RCC_PLLR_DIV_2);
    LL_RCC_PLL_Enable();
    LL_RCC_PLL_EnableDomain_SYS();

    timeout = 100000;
    while (LL_RCC_PLL_IsReady() != 1) {
        if (--timeout == 0) {
            return BSP_CLK_ERR_INIT;
        }
    }

    // Configure bus clock prescalers:
    // AHB  = DIV_1 -> HCLK  = 80 MHz
    // APB1 = DIV_1 -> PCLK1 = 80 MHz (for CAN1)
    // APB2 = DIV_1 -> PCLK2 = 80 MHz
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);

    // Switch system clock source to PLL
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    timeout = 100000;
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {
        if (--timeout == 0) {
            return BSP_CLK_ERR_INIT;
        }
    }

    // Update CMSIS SystemCoreClock variable
    SystemCoreClockUpdate();

    // Configure NVIC priority grouping to 4 bits for preemption priority (required by FreeRTOS on Cortex-M)
    #ifndef NVIC_PRIORITYGROUP_4
    #define NVIC_PRIORITYGROUP_4  0x00000003U
    #endif
    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    return BSP_CLK_OK;
}

uint32_t BSP_CLK_GetPCLK1Freq(void)
{
    LL_RCC_ClocksTypeDef rcc_clocks;
    LL_RCC_GetSystemClocksFreq(&rcc_clocks);
    return rcc_clocks.PCLK1_Frequency;
}
