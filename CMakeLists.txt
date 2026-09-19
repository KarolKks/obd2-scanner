cmake_minimum_required(VERSION 3.20)

# Set default toolchain file if not explicitly specified
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Cmake/arm-none-eabi-gcc.cmake" CACHE FILEPATH "ARM GCC Toolchain")
endif()

# Project definition
project(can-data-logger C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Hardware and FPU architecture flags
set(CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
)

# Target executable name
set(TARGET_ELF ${PROJECT_NAME}.elf)

# Target preprocessor definitions
add_definitions(
    -DSTM32L476xx
    -DUSE_FULL_LL_DRIVER
)

# Project build options
set(ENABLE_FREERTOS ON CACHE BOOL "Include FreeRTOS middleware" FORCE)

# Core sources
set(SOURCES
    Core/Src/main.c
    Core/Src/obd2.c
    Core/Src/obd_multiframe.c
    Core/Src/fatfs_sd.c
    Core/Src/system_stm32l4xx.c
    Core/Src/startup_stm32l476rgtx.s
)

# BSP sources
file(GLOB_RECURSE BSP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Bsp/Src/*.c"
)
list(APPEND SOURCES ${BSP_SOURCES})

# External Components sources (e.g. KY-040, SH1106, SN65HVD230)
file(GLOB_RECURSE COMPONENT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Components/*.c"
)
list(APPEND SOURCES ${COMPONENT_SOURCES})

# STM32L4xx LL Driver sources (excluding FMC, SDMMC and USB which require HAL headers)
set(LL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32L4xx_LL_Driver/Src")
list(APPEND SOURCES
    ${LL_DIR}/stm32l4xx_ll_adc.c
    ${LL_DIR}/stm32l4xx_ll_can.c
    ${LL_DIR}/stm32l4xx_ll_comp.c
    ${LL_DIR}/stm32l4xx_ll_crc.c
    ${LL_DIR}/stm32l4xx_ll_crs.c
    ${LL_DIR}/stm32l4xx_ll_dac.c
    ${LL_DIR}/stm32l4xx_ll_dma.c
    ${LL_DIR}/stm32l4xx_ll_dma2d.c
    ${LL_DIR}/stm32l4xx_ll_exti.c
    ${LL_DIR}/stm32l4xx_ll_gpio.c
    ${LL_DIR}/stm32l4xx_ll_i2c.c
    ${LL_DIR}/stm32l4xx_ll_lptim.c
    ${LL_DIR}/stm32l4xx_ll_lpuart.c
    ${LL_DIR}/stm32l4xx_ll_opamp.c
    ${LL_DIR}/stm32l4xx_ll_pka.c
    ${LL_DIR}/stm32l4xx_ll_pwr.c
    ${LL_DIR}/stm32l4xx_ll_rcc.c
    ${LL_DIR}/stm32l4xx_ll_rng.c
    ${LL_DIR}/stm32l4xx_ll_rtc.c
    ${LL_DIR}/stm32l4xx_ll_spi.c
    ${LL_DIR}/stm32l4xx_ll_swpmi.c
    ${LL_DIR}/stm32l4xx_ll_tim.c
    ${LL_DIR}/stm32l4xx_ll_usart.c
    ${LL_DIR}/stm32l4xx_ll_utils.c
)

# Middlewares - FatFs
set(FATFS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/FatFs")
list(APPEND SOURCES
    ${FATFS_DIR}/ff.c
    ${FATFS_DIR}/ffunicode.c
)

# Middlewares - FreeRTOS (optional)
if(ENABLE_FREERTOS)
    set(FREERTOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/FreeRTOS/Source")
    list(APPEND SOURCES
        ${FREERTOS_DIR}/croutine.c
        ${FREERTOS_DIR}/event_groups.c
        ${FREERTOS_DIR}/list.c
        ${FREERTOS_DIR}/queue.c
        ${FREERTOS_DIR}/stream_buffer.c
        ${FREERTOS_DIR}/tasks.c
        ${FREERTOS_DIR}/timers.c
        ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c
        ${FREERTOS_DIR}/portable/MemMang/heap_4.c
        ${FREERTOS_DIR}/CMSIS_RTOS_V2/cmsis_os2.c
    )
endif()

# Include directories
set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Bsp/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Components
    ${CMAKE_CURRENT_SOURCE_DIR}/Config
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32L4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32L4xx_LL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/FatFs
)

# Automatically add subdirectories of Components (and any Inc subfolders) to INCLUDE_DIRS
file(GLOB COMPONENT_SUBDIRS "${CMAKE_CURRENT_SOURCE_DIR}/Components/*")
foreach(SUBDIR ${COMPONENT_SUBDIRS})
    if(IS_DIRECTORY ${SUBDIR})
        list(APPEND INCLUDE_DIRS ${SUBDIR})
        if(EXISTS "${SUBDIR}/Inc")
            list(APPEND INCLUDE_DIRS "${SUBDIR}/Inc")
        endif()
    endif()
endforeach()

if(ENABLE_FREERTOS)
    list(APPEND INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/FreeRTOS/Source/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/FreeRTOS/Source/portable/GCC/ARM_CM4F
        ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/FreeRTOS/Source/CMSIS_RTOS_V2
    )
endif()

# Executable target
add_executable(${TARGET_ELF} ${SOURCES})

# Target include directories
target_include_directories(${TARGET_ELF} PRIVATE ${INCLUDE_DIRS})

# Target compile options
target_compile_options(${TARGET_ELF} PRIVATE
    ${CPU_FLAGS}
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
)

# Linker script configuration
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/Linker/STM32L476RGTX_FLASH.ld)

# Target link options
target_link_options(${TARGET_ELF} PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map,--cref
)

# Post-build artifacts (.hex, .bin, and size report)
add_custom_command(TARGET ${TARGET_ELF} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_ELF}> ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:${TARGET_ELF}> ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_ELF}>
    COMMENT "Generating HEX/BIN files and printing binary size..."
)
