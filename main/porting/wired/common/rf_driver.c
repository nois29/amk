/**
 * @file rf_driver.c
 */

#include <stdbool.h>
#include <string.h>
#include "rf_driver.h"
#include "amk_printf.h"
#include "amk_gpio.h"
#include "ring_buffer.h"
#include "usb_descriptors.h"

#ifndef RF_DRIVER_DEBUG
#define RF_DRIVER_DEBUG 1
#endif

#if RF_DRIVER_DEBUG
#define rf_driver_debug  amk_printf
#else
#define rf_driver_debug(...)
#endif

extern UART_HandleTypeDef huart1;

#define QUEUE_ITEM_SIZE   16                        // maximum size of the queue item
typedef struct {
    uint16_t    type;                               // type of the item
    uint16_t    size;                               // type of the item
    uint8_t     data[QUEUE_ITEM_SIZE];
} report_item_t;

#define QUEUE_SIZE      64
typedef struct {
    report_item_t   items[QUEUE_SIZE];
    uint32_t        head;
    uint32_t        tail;
} report_queue_t;

static report_queue_t report_queue;

static bool report_queue_empty(report_queue_t* queue)
{
    return queue->head == queue->tail;
}

static bool report_queue_full(report_queue_t* queue)
{
    return ((queue->tail + 1) % QUEUE_SIZE) == queue->head;
}

static void report_queue_init(report_queue_t* queue)
{
    memset(&queue->items[0], 0, sizeof(queue->items));
    queue->head = queue->tail = 0;
}

static bool report_queue_put(report_queue_t* queue, report_item_t* item)
{
    if (report_queue_full(queue)) return false;

    queue->items[queue->tail] = *item;
    queue->tail = (queue->tail + 1) % QUEUE_SIZE;
    return true;
}

static bool report_queue_get(report_queue_t* queue, report_item_t* item)
{
    if (report_queue_empty(queue)) return false;

    *item = queue->items[queue->head];
    queue->head = (queue->head + 1) % QUEUE_SIZE;
    return true;
}

static void process_data(uint8_t d);
static void enqueue_command(uint8_t *cmd);
static void process_report(report_item_t *item);
static uint8_t compute_checksum(uint8_t *data, uint32_t size);
static void rf_cmd_set_leds(uint8_t led);

static uint8_t command_buf[CMD_MAX_LEN];
static uint32_t command_buf_count = 0;

static uint8_t ring_buffer_data[128];
static ring_buffer_t ring_buffer;

static void process_data(uint8_t d)
{
    rf_driver_debug("uart received: %d, current count=%d\n", d, command_buf_count);
    if (command_buf_count == 0 && d != SYNC_BYTE_1) {
        rf_driver_debug("SYNC BYTE 1: %x\n", d);
        return;
    } else if (command_buf_count == 1 && d != SYNC_BYTE_2) {
        command_buf_count = 0;
        memset(command_buf, 0, sizeof(command_buf));
        rf_driver_debug("SYNC BYTE 2: %x\n", d);
        return;
    }

    if (command_buf_count >= CMD_MAX_LEN) {
        rf_driver_debug("UART command oversize\n");
        memset(command_buf, 0, sizeof(command_buf));
        command_buf_count = 0;
        return;
    }

    command_buf[command_buf_count] = d;

    if (command_buf[2]+2 > CMD_MAX_LEN) {
        rf_driver_debug("UART invalid command size: %d\n", command_buf[2]);
        memset(command_buf, 0, sizeof(command_buf));
        command_buf_count= 0;
        return;
    }

    command_buf_count++;
    if (command_buf_count > 2) {
        if (command_buf_count == (command_buf[2] + 2)) {
            // full packet received
            enqueue_command(&command_buf[2]);
            memset(command_buf, 0, sizeof(command_buf));
            command_buf_count = 0;
        }
    }
}

static void enqueue_command(uint8_t *cmd)
{
    uint8_t checksum = compute_checksum(cmd + 2, cmd[0] - 2);
    if (checksum != cmd[1]) {
        // invalid checksum
        rf_driver_debug("Checksum: LEN:%x, SRC:%x, CUR:%x\n", cmd[0], cmd[1], checksum);
        return;
    }
    if ((cmd[2] != CMD_SET_LEDS) && (cmd[0] != 4)){
        rf_driver_debug("Command: not a valid command:%d or length:%d\n", cmd[2], cmd[0]);
        return;
    }

    // set leds
    rf_cmd_set_leds(cmd[3]);
}

static void process_report(report_item_t *item)
{
    static uint8_t command[128];
    uint8_t checksum = item->type;
    checksum += compute_checksum(&item->data[0], item->size);
    command[0] = SYNC_BYTE_1;
    command[1] = SYNC_BYTE_2;
    command[2] = item->size+3;
    command[3] = checksum;
    command[4] = item->type;
    for (uint32_t i = 0; i < item->size; i++) {
        command[5+i] = item->data[i];
    }

    HAL_UART_Transmit(&huart1, command, item->size+5, 10);
}

static uint8_t compute_checksum(uint8_t *data, uint32_t size)
{
    uint8_t checksum = 0;
    for (uint32_t i = 0; i < size; i++) {
        checksum += data[i];
    }
    return checksum;
}

static void rf_cmd_set_leds(uint8_t led)
{
    rf_driver_debug("Led set state:%d\n", led);
}

void rf_driver_init(void)
{
    // turn on rf power
    gpio_set_output_pushpull(RF_POWER_PIN);
    gpio_write_pin(RF_POWER_PIN, 1);

    // enable rf module
    gpio_set_output_pushpull(RF_WAKEUP_PIN);
    gpio_write_pin(RF_WAKEUP_PIN, 1);

    // pin for checking rf state
    gpio_set_input_pulldown(RF_READY_PIN);

    report_queue_init(&report_queue);

    memset(command_buf, 0, sizeof(command_buf));
    command_buf_count = 0;
    rb_init(&ring_buffer, ring_buffer_data, 128);

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
}

void rf_driver_task(void)
{
    while (rb_used_count(&ring_buffer) > 0) {
        uint8_t c = 0;
        __disable_irq();
        c = rb_read_byte(&ring_buffer);
        __enable_irq();

        process_data(c);
    }

    if (gpio_read_pin(RF_READY_PIN)) {
        report_item_t item;
        while (report_queue_get(&report_queue, &item)) {
            process_report(&item);
        }
    }
}

void uart_recv_char(uint8_t c)
{
    rb_write_byte(&ring_buffer, c);
}

void rf_driver_put_report(uint32_t type, void* data, uint32_t size)
{
    report_item_t item;
    memcpy(&item.data[0], data, size);
    item.size = size;
    item.type = type;

    report_queue_put(&report_queue, &item);
}

void rf_driver_toggle(void)
{
    if (usb_setting & OUTPUT_RF) {
        usb_setting &= ~OUTPUT_RF;
        rf_driver_debug("disable rf output\n");
    } else {
        usb_setting |= OUTPUT_RF;
        rf_driver_debug("enable rf output\n");
    }
}