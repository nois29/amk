/**
 * @file apc_matrix.c
 */

#include "matrix.h"
#include "wait.h"
#include "timer.h"
#include "led.h"
#include "amk_apc.h"

#include "amk_gpio.h"
#include "amk_utils.h"
#include "amk_printf.h"

#ifndef CUSTOM_MATRIX_DEBUG
#define CUSTOM_MATRIX_DEBUG 1
#endif

#if CUSTOM_MATRIX_DEBUG
#define custom_matrix_debug  amk_printf
#else
#define custom_matrix_debug(...)
#endif


#define COL_A_MASK  0x01
#define COL_B_MASK  0x02
#define COL_C_MASK  0x04
#define COL_D_MASK  0x08
#define ADC_TIMEOUT 10

extern ADC_HandleTypeDef    hadc1;
DMA_HandleTypeDef           hdma_adc1;
static volatile uint32_t    adc1_dma_done;
#define ADC_CHANNEL_COUNT   2

static pin_t custom_row_pins[] = MATRIX_ROW_PINS;
static pin_t custom_col_pins[] = MATRIX_COL_PINS;

static bool adc_read(uint32_t *data, uint32_t timeout)
{
    adc1_dma_done = 0;
    if( HAL_ADC_Start_DMA(&hadc1, data, ADC_CHANNEL_COUNT) == HAL_OK) {
        uint32_t start = timer_read32();
        while( !adc1_dma_done) {
            if (timer_elapsed32(start) > timeout) {
                custom_matrix_debug("adc_read timeout.\n");
                return false;
            }
        }
    } else {
        custom_matrix_debug("adc_read Start_DMA failed.\n");
        return false;
    }

    return true;
}

static uint8_t right_col_map[] = {14, 15, 16, 13, 9, 12, 11, 10};

static void sense_key(bool* left, bool *right, uint32_t row, uint32_t col)
{
    static uint32_t adc_value[ADC_CHANNEL_COUNT];

    if ( !adc_read(adc_value, ADC_TIMEOUT)) {
        return ;
    }
    
    *left = apc_matrix_update(row, col, adc_value[0]);
    if (right_col_map[col] != 16)
        *right = apc_matrix_update(row, right_col_map[col]-1, adc_value[1]);

    if (*left) {
        custom_matrix_debug("key down: data=%d, row=%d, col=%d\n", adc_value[0], row, col);
    }

    if (*right) {
        custom_matrix_debug("key down: data=%d, row=%d, col=%d\n", adc_value[1], row, col+8);
    }
}

void matrix_init_custom(void)
{
#ifdef RGB_EN_PIN
    gpio_set_output_pushpull(RGB_EN_PIN);
    gpio_write_pin(RGB_EN_PIN, 1);
#endif

    //power on switch board
#ifdef POWER_PIN
    gpio_set_output_pushpull(POWER_PIN);
    gpio_write_pin(POWER_PIN, 1);

    wait_ms(100);
#endif

    // initialize row pins
    for (int i = 0; i < AMK_ARRAY_SIZE(custom_row_pins); i++) {
        gpio_set_output_pushpull(custom_row_pins[i]);
        gpio_write_pin(custom_row_pins[i], 0);
    }

    // initialize col pins
#ifdef LEFT_EN_PIN
    gpio_set_output_pushpull(LEFT_EN_PIN);
    gpio_write_pin(LEFT_EN_PIN, 1);
#endif

#ifdef RIGHT_EN_PIN
    gpio_set_output_pushpull(RIGHT_EN_PIN);
    gpio_write_pin(RIGHT_EN_PIN, 1);
#endif

    gpio_set_output_pushpull(COL_A_PIN);
    gpio_write_pin(COL_A_PIN, 0);
    gpio_set_output_pushpull(COL_B_PIN);
    gpio_write_pin(COL_B_PIN, 0);
    gpio_set_output_pushpull(COL_C_PIN);
    gpio_write_pin(COL_C_PIN, 0);

    apc_matrix_init();
}



bool matrix_scan_custom(matrix_row_t* raw)
{
    bool changed = false;
#if SCAN_ONE
    changed = scan_one(raw);
#else
    for (int row = 0; row < MATRIX_ROWS; row++) {
        matrix_row_t last_row_value    = raw[row];
        matrix_row_t current_row_value = last_row_value;
        gpio_write_pin(custom_row_pins[row], 1);
        //wait_ms(1);
        wait_us(500);

        for (int col = 0; col < 8/*MATRIX_COLS*/; col++) {

            gpio_write_pin(COL_A_PIN, (custom_col_pins[col]&COL_A_MASK) ? 1 : 0);
            gpio_write_pin(COL_B_PIN, (custom_col_pins[col]&COL_B_MASK) ? 1 : 0);
            gpio_write_pin(COL_C_PIN, (custom_col_pins[col]&COL_C_MASK) ? 1 : 0);

            //if (custom_col_pins[col]&L_MASK) {
                gpio_write_pin(LEFT_EN_PIN,  0);
            //}

            //if (custom_col_pins[col]&R_MASK) {
                gpio_write_pin(RIGHT_EN_PIN, 0);
            //}
            bool left = false;
            bool right = false;

            sense_key(&left, &right, row, col);
            if (left) {
                current_row_value |= (1 << col);
            } else {
                current_row_value &= ~(1 << col);
            }

            if (right_col_map[col] != 16) {
                if (right) {
                    current_row_value |= (1 << (right_col_map[col]-1));
                } else {
                    current_row_value &= ~(1 << (right_col_map[col]-1));
                }
            }

            if (last_row_value != current_row_value) {
                raw[row] = current_row_value;
                changed = true;
            }
        }

        gpio_write_pin(LEFT_EN_PIN,  1);
        gpio_write_pin(RIGHT_EN_PIN, 1);
        gpio_write_pin(custom_row_pins[row], 0);
        wait_us(10);
    }
#endif

    if (changed) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            custom_matrix_debug("row:%d-%x\n", row, matrix_get_row(row));
        }
    }

    //return false;

    return changed;
}

int adc_init_kb(void)
{
    HAL_NVIC_SetPriority(DMA2_Stream4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream4_IRQn);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = ENABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 2;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = 2;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }

    return 1;
}

int adc_msp_init_kb(void)
{ 
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ADC1 DMA Init */
    hdma_adc1.Instance = DMA2_Stream4;
    hdma_adc1.Init.Channel = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_adc1.Init.Mode = DMA_NORMAL;
    hdma_adc1.Init.Priority = DMA_PRIORITY_LOW;
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) {
        Error_Handler();
    }

    __HAL_LINKDMA(&hadc1,DMA_Handle,hdma_adc1);
    return 1;
}

void DMA2_Stream4_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* AdcHandle)
{
    adc1_dma_done = 1;
}
