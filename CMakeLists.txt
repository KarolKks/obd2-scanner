cmake_minimum_required(VERSION 3.20)

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

# Project source files
set(SOURCES
    Src/main.c
    Src/system_stm32l4xx.c
    Src/startup_stm32l476rgtx.s
    Middlewares/FatFs/ff.c
    Middlewares/FatFs/diskio.c
    Middlewares/FatFs/ffunicode.c
)

# Include directories
set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32L4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32L4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/FatFs
)

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
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32L476RGTX_FLASH.ld)

# Target link options
target_link_options(${TARGET_ELF} PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    --specs=nano.specs
    -Wl,-Map=${PROJECT_NAME}.map,--cref
)

# Post-build artifacts (.hex, .bin, and size report)
add_custom_command(TARGET ${TARGET_ELF} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_ELF}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:${TARGET_ELF}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_ELF}>
    COMMENT "Generating HEX/BIN files and printing binary size..."
)
