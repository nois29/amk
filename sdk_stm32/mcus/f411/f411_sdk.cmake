
target_sources(${KEYBOARD}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/startup_stm32f411xe.s
    ${CMAKE_CURRENT_LIST_DIR}/${SDK_MCU_FAMILY}_hal_timebase_tim.c
    ${VENDOR_DIR}/driver_${SDK_MCU_SERIES}/Src/${SDK_MCU_FAMILY}_hal_flash_ramfunc.c
    ${VENDOR_DIR}/driver_${SDK_MCU_SERIES}/Src/${SDK_MCU_FAMILY}_hal_dma_ex.c
    ${VENDOR_DIR}/driver_${SDK_MCU_SERIES}/Src/${SDK_MCU_FAMILY}_hal_pwr_ex.c
    ${VENDOR_DIR}/driver_${SDK_MCU_SERIES}/Src/${SDK_MCU_FAMILY}_hal_i2c.c
    ${VENDOR_DIR}/driver_${SDK_MCU_SERIES}/Src/${SDK_MCU_FAMILY}_hal_i2c_ex.c
    ${VENDOR_DIR}/driver_${SDK_MCU_SERIES}/Src/${SDK_MCU_FAMILY}_hal_spi.c
    )

set(MCU_FLAGS -mcpu=cortex-m4 -mthumb -mabi=aapcs -mfloat-abi=hard -mfpu=fpv4-sp-d16 CACHE STRING INTERNAL)

target_compile_definitions(${KEYBOARD} 
    PRIVATE
    SPI_USE_INSTANCE_1
	SPI_USE_INSTANCE_2
	STM32F411xE
    CFG_TUSB_MCU=OPT_MCU_STM32F4
	SYSTEM_CLOCK=96000000
    )

set(SDK_MCU_LD STM32F411CEUx)
