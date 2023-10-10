/*
This goes below
#include "via.h"
*/

#include "qmk_version.h"
#include "signalrgb_via.h"
#include "via.h"
#include "host.h"
#include "raw_hid.h"
#include "rgb_matrix.h"

#include "amk_printf.h"
#ifndef SIGNALRGB_DEBUG
#define SIGNALRGB_DEBUG 1
#endif

#if SIGNALRGB_DEBUG 
#define signalrgb_debug  amk_printf
#else
#define signalrgb_debug(...)
#endif

/*
This goes below
// Called by QMK core to process VIA-specific keycodes.
*/
uint8_t packet[32];

 void get_qmk_version(void) //Grab the QMK Version
{
        packet[0] = id_signalrgb_qmk_version;
        packet[1] = QMK_VERSION_BYTE_1;
        packet[2] = QMK_VERSION_BYTE_2;
        packet[3] = QMK_VERSION_BYTE_3;

        raw_hid_send(packet, 32);
}

void get_signalrgb_protocol_version(void) //Grab what version of the SignalRGB protocol a keyboard is running
{
        packet[0] = id_signalrgb_protocol_version;
        packet[1] = PROTOCOL_VERSION_BYTE_1;
        packet[2] = PROTOCOL_VERSION_BYTE_2;
        packet[3] = PROTOCOL_VERSION_BYTE_3;

        raw_hid_send(packet, 32);
}

void get_unique_identifier(void) //Grab the unique identifier for each specific model of keyboard.
{
        packet[0] = id_signalrgb_unique_identifier;
        packet[1] = DEVICE_UNIQUE_IDENTIFIER_BYTE_1;
        packet[2] = DEVICE_UNIQUE_IDENTIFIER_BYTE_2;
        packet[3] = DEVICE_UNIQUE_IDENTIFIER_BYTE_3;

        raw_hid_send(packet, 32);
}

void led_streaming(uint8_t *data) //Stream data from HID Packets to Keyboard.
{
    if (rgb_matrix_get_mode() != RGB_MATRIX_CUSTOM_SIGNALRGB) {
        rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_SIGNALRGB);
    }
    
    uint8_t index = data[1];
    uint8_t numberofleds = data[2]; 

    //signalrgb_debug("signalrgb: led streaming, index=%d, count=%d\n", index, numberofleds);


    if(numberofleds >= 10)
    {
        packet[1] = DEVICE_ERROR_LEDS;
        raw_hid_send(packet,32);
        return; 
    } 
    
    for (uint8_t i = 0; i < numberofleds; i++)
    {
      uint8_t offset = (i * 3) + 3;
      uint8_t  r = data[offset];
      uint8_t  g = data[offset + 1];
      uint8_t  b = data[offset + 2];

      //if ( ((index + i) == CAPS_LOCK_LED_INDEX && host_keyboard_led_state().caps_lock) || ((index + i) == NUM_LOCK_LED_INDEX && host_keyboard_led_state().num_lock) || ((index + i) == SCROLL_LOCK_LED_INDEX && host_keyboard_led_state().scroll_lock))   {
      //if ( ((index + i) == CAPS_LOCK_LED_INDEX && host_keyboard_led_state().caps_lock) || ((index + i) == NUM_LOCK_LED_INDEX && host_keyboard_led_state().num_lock))   {
      //if ( (index + i) == CAPS_MAC_WIN_LED_INDEX && host_keyboard_led_state().caps_lock)   {
      //if ( (index + i) == CAPS_LOCK_LED_INDEX && host_keyboard_led_state().caps_lock)   {
      //if ( (index + i) == NUM_LOCK_LED_INDEX && host_keyboard_led_state().num_lock)  {
      //#if defined(RGBLIGHT_ENABLE)
      //rgblight_setrgb_at(255, 255, 255, index + i);
      //#elif defined(RGB_MATRIX_ENABLE)
      //rgb_matrix_set_color(index + i, 255, 255, 255);
      //#endif

      //} else {

      #if defined(RGBLIGHT_ENABLE)
      rgblight_setrgb_at(r, g, b, index + i);
      #elif defined(RGB_MATRIX_ENABLE)
      rgb_matrix_set_color(index + i, r, g, b);
      #endif
        }
     }
//}

void signalrgb_mode_enable(void)
{
    #if defined(RGB_MATRIX_ENABLE)
    rgb_matrix_mode_noeeprom(RGB_MATRIX_CUSTOM_SIGNALRGB); //Set RGB Matrix to SignalRGB Compatible Mode
    signalrgb_debug("signalrgb: set mode:%d\n", RGB_MATRIX_CUSTOM_SIGNALRGB);
    #endif
}

void signalrgb_mode_disable(void)
{
    #if defined(RGBLIGHT_ENABLE)
    rgblight_reload_from_eeprom();
    #elif defined(RGB_MATRIX_ENABLE)
    rgb_matrix_reload_from_eeprom(); //Reloading last effect from eeprom
    signalrgb_debug("signalrgb: disable mode:%d\n", RGB_MATRIX_CUSTOM_SIGNALRGB);
    #endif
}

void signalrgb_total_leds(void)//Grab total number of leds that a board has.
{
    packet[0] = id_signalrgb_total_leds;
    #if defined(RGBLIGHT_ENABLE)
    packet[1] = RGBLED_NUM;
    #elif defined(RGB_MATRIX_ENABLE)
    packet[1] = RGB_MATRIX_LED_COUNT;
    #endif
    raw_hid_send(packet,32);
}

void signalrgb_firmware_type(void) //Grab which fork of qmk a board is running.
{
    packet[0] = id_signalrgb_firmware_type;
    packet[1] = FIRMWARE_TYPE_BYTE;
    raw_hid_send(packet,32);
}


/*
Part of void raw_hid_receive(uint8_t *data, uint8_t length) {
specifically in (*command_id)
right before protocol version

*/
void raw_hid_receive_kb(uint8_t *data, uint8_t length)
{
    uint8_t command_id = data[0];
    switch(command_id) {
        case id_signalrgb_qmk_version:
        {
            get_qmk_version();
            break;
        }

        case id_signalrgb_protocol_version:
        {
            get_signalrgb_protocol_version();
            break;
        }

        case id_signalrgb_unique_identifier:
        {
            get_unique_identifier();
            break;
        }

        case id_signalrgb_stream_leds: 
        {
            led_streaming(data);
            break;
        }

        case id_signalrgb_effect_enable: 
        {
            signalrgb_mode_enable();
            break;
        }

        case id_signalrgb_effect_disable: 
        {
            signalrgb_mode_disable();
            break;
        }

        case id_signalrgb_total_leds:
        {
            signalrgb_total_leds();
            break;
        }

        case id_signalrgb_firmware_type:
        {
            signalrgb_firmware_type();
            break;
        }
        default:
        {
            data[0] = id_unhandled;
            break;
        }
    }
}