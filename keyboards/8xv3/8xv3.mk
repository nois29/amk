
MCU = STM32F411
TINYUSB_ENABLE = yes
TINYUSB_USE_HAL = yes
RGB_MATRIX_ENABLE = yes
RGB_LINEAR_ENABLE = yes
EECONFIG_FLASH = yes
#SCREEN_ENABLE = yes
MSC_ENABLE = yes
DYNAMIC_CONFIGURATION = yes
VIAL_ENABLE = yes

UF2_ENABLE = yes
UF2_FAMILY = STM32F4

LINKER_PATH = $(KEYBOARD_DIR)

SRCS += $(KEYBOARD_DIR)/8xv3.c
#SRCS += $(KEYBOARD_DIR)/custom_matrix.c
#SRCS += $(KEYBOARD_DIR)/8xv3_keymap.c
SRCS += $(KEYBOARD_DIR)/cm2.c
SRCS += $(KEYBOARD_DIR)/8xv3_gcmap.c

SRCS += $(KEYBOARD_DIR)/display.c
SRCS += $(MAIN_DIR)/drivers/st7735.c

#SRCS += $(KEYBOARD_DIR)/ds2.c
#SRCS += $(MAIN_DIR)/drivers/gc9107.c
