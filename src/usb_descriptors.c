/*
 * Copyright (c) 2022 HASUMI Hitoshi | MIT License
 *
 * This program was originally written under the copyright below
 * and modified by the author.
 */
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "bsp/board_api.h"
#include "tusb.h"

#include <mrubyc.h>
#include "../include/usb_descriptors.h"
#include "../include/raw_hid.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

#define USB_VID   0x16c0
//#define USB_BCD   0x0200
#define USB_BCD   0x0110

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = USB_BCD,

  // Use Interface Association Descriptor (IAD) for CDC
  // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,

  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor           = USB_VID,
  .idProduct          = USB_PID,
  .bcdDevice          = 0x0100,

  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,

  .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum
{
  ITF_NUM_CDC_0 = 0,
  ITF_NUM_CDC_0_DATA,
  ITF_NUM_CDC_1,
  ITF_NUM_CDC_1_DATA,
  ITF_NUM_MSC,
  ITF_NUM_HID,
  ITF_NUM_TOTAL
};

#if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX
  // LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
  // 0 control, 1 In, 2 Bulk, 3 Iso, 4 In etc ...
  #define EPNUM_CDC_0_NOTIF   0x81
  #define EPNUM_CDC_0_OUT     0x02
  #define EPNUM_CDC_0_IN      0x82

  #define EPNUM_CDC_1_NOTIF   0x84
  #define EPNUM_CDC_1_OUT     0x05
  #define EPNUM_CDC_1_IN      0x85

  #define EPNUM_MSC_OUT       0x08
  #define EPNUM_MSC_IN        0x88

#elif CFG_TUSB_MCU == OPT_MCU_CXD56
  // CXD56 USB driver has fixed endpoint type (bulk/interrupt/iso) and direction (IN/OUT) by its number
  // 0 control (IN/OUT), 1 Bulk (IN), 2 Bulk (OUT), 3 In (IN), 4 Bulk (IN), 5 Bulk (OUT), 6 In (IN)
  #define EPNUM_CDC_0_NOTIF   0x83
  #define EPNUM_CDC_0_OUT     0x02
  #define EPNUM_CDC_0_IN      0x81

  #define EPNUM_CDC_1_NOTIF   0x86
  #define EPNUM_CDC_1_OUT     0x05
  #define EPNUM_CDC_1_IN      0x84

  #define EPNUM_MSC_OUT       0x07
  #define EPNUM_MSC_IN        0x08

#elif defined(TUD_ENDPOINT_ONE_DIRECTION_ONLY)
  // MCUs that don't support a same endpoint number with different direction IN and OUT
  //    e.g EP1 OUT & EP1 IN cannot exist together
  #define EPNUM_CDC_0_NOTIF   0x81
  #define EPNUM_CDC_0_OUT     0x02
  #define EPNUM_CDC_0_IN      0x83

  #define EPNUM_CDC_1_NOTIF   0x84
  #define EPNUM_CDC_1_OUT     0x05
  #define EPNUM_CDC_1_IN      0x86

  #define EPNUM_MSC_OUT       0x07
  #define EPNUM_MSC_IN        0x88

#else
  #define EPNUM_CDC_0_NOTIF   0x81
  #define EPNUM_CDC_0_OUT     0x02
  #define EPNUM_CDC_0_IN      0x82

  #define EPNUM_CDC_1_NOTIF   0x83
  #define EPNUM_CDC_1_OUT     0x04
  #define EPNUM_CDC_1_IN      0x84

  #define EPNUM_MSC_OUT       0x05
  #define EPNUM_MSC_IN        0x85

  #define EPNUM_HID_OUT       0x06
  #define EPNUM_HID_IN        0x86

#endif

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+
uint8_t const desc_hid_report[] =
{
  TUD_HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID(REPORT_ID_KEYBOARD         )),
  TUD_HID_REPORT_DESC_MOUSE   ( HID_REPORT_ID(REPORT_ID_MOUSE            )),
  TUD_HID_REPORT_DESC_CONSUMER( HID_REPORT_ID(REPORT_ID_CONSUMER_CONTROL )),
  RAW_HID_REPORT_DESC(          HID_REPORT_ID(REPORT_ID_RAWHID           )),
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + CFG_TUD_CDC * TUD_CDC_DESC_LEN + TUD_MSC_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

// full speed configuration
uint8_t const desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),

  // 2nd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, 6, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_OUT, EPNUM_CDC_1_IN, 64),

  // Interface number, string index, EP Out & EP In address, EP size
  TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),

  // HID: Interface number, string index, protocol, report descriptor len, EP In address, EP Out address, size & polling interval
  TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID, 7, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID_IN, EPNUM_HID_OUT, 64, 0x08),
};

#if TUD_OPT_HIGH_SPEED
// Per USB specs: high speed capable device must report device_qualifier and other_speed_configuration

// high speed configuration
uint8_t const desc_hs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 512),

  // 2nd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, 6, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_OUT, EPNUM_CDC_1_IN, 512),

  // Interface number, string index, EP Out & EP In address, EP size
  TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 512),

  // HID: Interface number, string index, protocol, report descriptor len, EP In address, EP Out address, size & polling interval
  TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID, 7, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID_IN, EPNUM_HID_OUT, 64, 0x08),
};

// other speed configuration
uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN];

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
tusb_desc_device_qualifier_t const desc_device_qualifier =
{
  .bLength            = sizeof(tusb_desc_device_qualifier_t),
  .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
  .bcdUSB             = USB_BCD,

  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,

  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .bNumConfigurations = 0x01,
  .bReserved          = 0x00
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
uint8_t const* tud_descriptor_device_qualifier_cb(void)
{
  return (uint8_t const*) &desc_device_qualifier;
}

// Invoked when received GET OTHER SEED CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  // if link speed is high return fullspeed config, and vice versa
  // Note: the descriptor type is OHER_SPEED_CONFIG instead of CONFIG
  memcpy(desc_other_speed_config,
         (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration : desc_hs_configuration,
         CONFIG_TOTAL_LEN);

  desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;

  return desc_other_speed_config;
}

#endif // highspeed


// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

#if TUD_OPT_HIGH_SPEED
  // Although we are highspeed, host may be fullspeed.
  return (tud_speed_get() == TUSB_SPEED_HIGH) ?  desc_hs_configuration : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// String Descriptor Index
enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
};

// array of pointer to string descriptors
char const *string_desc_arr[] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "PicoRuby",                    // 1: Manufacturer
  "R2P2",                        // 2: Product
  NULL,                          // 3: Serials will use unique ID if possible
  "PicoRuby CDC",                // 4: CDC Interface 0 (Application)
  "PicoRuby MSC",                // 5: MSC Interface
  "PicoRuby CDC Debug",          // 6: CDC Interface 1 (Debug)
  "PicoRuby HID",                // 7: HID Interface
};

static uint16_t _desc_str[32 + 1];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t chr_count;

  switch ( index ) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
      // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

      if ( !(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) ) {
        //printf("Invalid string index: %d\n", index);
        return NULL;
      }


      const char *str = string_desc_arr[index];

      // Cap at max char
      chr_count = strlen(str);
      size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type
      if ( chr_count > max_count ) chr_count = max_count;

      // Convert ASCII string into UTF-16
      for ( size_t i = 0; i < chr_count; i++ ) {
        _desc_str[1 + i] = (uint16_t)str[i];
      }
      break;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}

//--------------------------------------------------------------------+
// HID Callbacks and State
//--------------------------------------------------------------------+

static uint8_t raw_hid_last_received_report[REPORT_RAW_MAX_LEN];
static uint8_t raw_hid_last_received_report_length = 0;
static bool raw_hid_report_received = false;
static bool observing_output_report = false;
static uint8_t keyboard_output_report = 0;

#define HID0_REPORT_ID_BITS ( \
    (1<<REPORT_ID_KEYBOARD) | (1<<REPORT_ID_MOUSE) | \
    (1<<REPORT_ID_CONSUMER_CONTROL) | (1<<REPORT_ID_RAWHID) )

static uint8_t input_updated_bitmap = 0;
static uint8_t keyboard_modifier = 0;
static uint8_t keyboard_keycodes[6] = {0, 0, 0, 0, 0, 0};
static uint16_t consumer_keycode = 0;
static bool via_active = false;

typedef struct mouse_values {
  uint8_t buttons;
  int8_t x;
  int8_t y;
  int8_t vertical;
  int8_t horizontal;
} MouseValues;
static MouseValues mouse;

// Invoked when received GET HID REPORT DESCRIPTOR
uint8_t const *
tud_hid_descriptor_report_cb(uint8_t instance)
{
  (void) instance;
  return desc_hid_report;
}

// Invoked when received GET_REPORT control request
uint16_t
tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;
  return 0;
}

// Invoked when received SET_REPORT control request or received data on OUT endpoint
void
tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;

  if (report_type == HID_REPORT_TYPE_INVALID) {
    report_id = buffer[0];
    buffer++;
    bufsize--;
  } else if(report_type != HID_REPORT_TYPE_OUTPUT && report_type != HID_REPORT_TYPE_FEATURE) {
    return;
  }

  if (bufsize < 1) return;

  switch (report_id) {
    case REPORT_ID_KEYBOARD:
      if (observing_output_report) {
        keyboard_output_report = buffer[0];
      }
      break;
    case REPORT_ID_RAWHID:
      memcpy(raw_hid_last_received_report, buffer, bufsize);
      raw_hid_last_received_report_length = bufsize;
      raw_hid_report_received = true;
      break;
    default:
      break;
  }
}

// Invoked when received SET_PROTOCOL request
void
tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol)
{
  (void) instance;
  (void) protocol;
}

static void
send_hid_report(void)
{
  // skip if report is not updated
  if (input_updated_bitmap) {
    // skip if hid is not ready yet
    if (!tud_hid_n_ready(0)) {
      input_updated_bitmap &= ~(HID0_REPORT_ID_BITS);
    }
  } else {
    return;
  }

  for(uint8_t i = 1; i < REPORT_ID_COUNT; i++) {
    if(input_updated_bitmap & (1<<i)) {
      switch(i)
      {
        case REPORT_ID_KEYBOARD: {
          tud_hid_n_keyboard_report(0, REPORT_ID_KEYBOARD, keyboard_modifier, keyboard_keycodes);
        }
        break;

        case REPORT_ID_MOUSE: {
          tud_hid_n_mouse_report(0, REPORT_ID_MOUSE,
            mouse.buttons,
            mouse.x,
            mouse.y,
            mouse.vertical,
            mouse.horizontal
          );
          memset(&mouse, 0, sizeof(MouseValues));
        }
        break;

        case REPORT_ID_CONSUMER_CONTROL: {
          if(!via_active) {
            tud_hid_n_report(0, REPORT_ID_CONSUMER_CONTROL, &consumer_keycode, 2);
          }
        }
        break;

        default: break;
      }
      return;
    }
  }
}

// Invoked when report is sent
void
tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) len;

  input_updated_bitmap &= ~(1<<report[0]); // report[0] is report ID
  send_hid_report();
}

//--------------------------------------------------------------------+
// Ruby methods for USB HID
//--------------------------------------------------------------------+

static void
c_start_observing_output_report(mrbc_vm *vm, mrbc_value *v, int argc)
{
  (void) vm;
  (void) v;
  (void) argc;
  observing_output_report = true;
}

static void
c_stop_observing_output_report(mrbc_vm *vm, mrbc_value *v, int argc)
{
  (void) vm;
  (void) v;
  (void) argc;
  observing_output_report = false;
}

static void
c_output_report(mrbc_vm *vm, mrbc_value *v, int argc)
{
  (void) v;
  (void) argc;
  SET_INT_RETURN(keyboard_output_report);
}

static void
c_hid_task(mrbc_vm *vm, mrbc_value *v, int argc)
{
  (void) vm;
  (void) v;
  (void) argc;

  static bool mouse_zero_report = false;
  if (mouse.x != 0 ||
      mouse.y != 0 ||
      0 < mouse.buttons ||
      mouse.vertical != 0 ||
      mouse.horizontal != 0)
  {
    input_updated_bitmap |= 1<<REPORT_ID_MOUSE;
    mouse_zero_report = false;
  } else if (!mouse_zero_report) {
    input_updated_bitmap |= 1<<REPORT_ID_MOUSE;
    mouse_zero_report = true;
  }

  if (consumer_keycode != 0) {
    via_active = false;
  }

  if (tud_suspended()) {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }

  send_hid_report();
}

static void
c_raw_hid_report_received_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  (void) v;
  (void) argc;
  if(raw_hid_report_received) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_get_last_received_raw_hid_report(mrbc_vm *vm, mrbc_value *v, int argc)
{
  (void) v;
  (void) argc;
  mrbc_value rb_val_array = mrbc_array_new(vm, REPORT_RAW_MAX_LEN);
  mrbc_array *rb_array = rb_val_array.array;

  rb_array->n_stored = raw_hid_last_received_report_length;
  for(uint8_t i=0; i<raw_hid_last_received_report_length && i<REPORT_RAW_MAX_LEN; i++) {
    mrbc_set_integer( (rb_array->data)+i, raw_hid_last_received_report[i] );
  }
  raw_hid_report_received = false;

  SET_RETURN(rb_val_array);
}

static bool
report_raw_hid(uint8_t* data, uint8_t len)
{
  // Remote wakeup
  if (tud_suspended()) {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }
  /*------------- RAW HID -------------*/
  if (tud_hid_n_ready(0)) {
    via_active = true;
    return tud_hid_n_report(0, REPORT_ID_RAWHID, data, len);
  } else {
    return false;
  }
}

static void
c_report_raw_hid(mrbc_vm *vm, mrbc_value *v, int argc)
{
  (void) vm;
  mrbc_array rb_ary = *( GET_ARY_ARG(1).array );
  uint8_t c_data[REPORT_RAW_MAX_LEN];
  uint8_t len = REPORT_RAW_MAX_LEN;

  memset(c_data, 0, REPORT_RAW_MAX_LEN);
  if(GET_ARY_ARG(1).tt == MRBC_TT_ARRAY) {
    if(rb_ary.n_stored<len) {
      len = rb_ary.n_stored;
    }
    for(uint8_t i=0; i<len; i++) {
      c_data[i] = mrbc_integer(rb_ary.data[i]);
    }
  }

  if( report_raw_hid(c_data, len) ) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_merge_keyboard_report(mrbc_vm *vm, mrbc_value *v, int argc)
{
  (void) vm;

  if(keyboard_modifier != GET_INT_ARG(1)) {
    keyboard_modifier = (uint8_t)GET_INT_ARG(1);
    input_updated_bitmap |= 1<<REPORT_ID_KEYBOARD;
  }

  mrbc_array keycodes = *(GET_ARY_ARG(2).array);
  char keycodes_join[6];
  for (int i = 0; i < 6; i++) {
    if (i < keycodes.n_stored) {
      keycodes_join[i] = mrbc_integer(keycodes.data[i]);
    } else {
      keycodes_join[i] = 0;
    }
  }
  if(memcmp(keyboard_keycodes, keycodes_join, 6)) {
    memcpy(keyboard_keycodes, keycodes_join, 6);
    input_updated_bitmap |= 1<<REPORT_ID_KEYBOARD;
  }

  SET_NIL_RETURN();
}

static void
c_merge_consumer_report(mrbc_vm *vm, mrbc_value *v, int argc)
{
  (void) vm;
  (void) v;

  if(consumer_keycode != GET_INT_ARG(1)) {
    consumer_keycode = (uint16_t)GET_INT_ARG(1);
    input_updated_bitmap |= 1<<REPORT_ID_CONSUMER_CONTROL;
  }

  SET_NIL_RETURN();
}

static void
c_merge_mouse_report(mrbc_vm *vm, mrbc_value *v, int argc)
{
  (void) vm;
  (void) v;
  mouse.buttons    |= GET_INT_ARG(1);
  mouse.x          += GET_INT_ARG(2);
  mouse.y          += GET_INT_ARG(3);
  mouse.horizontal += GET_INT_ARG(4);
  mouse.vertical   += GET_INT_ARG(5);
  SET_NIL_RETURN();
}

void
USB_hid_init(void)
{
  mrbc_class *mrbc_class_USB = mrbc_define_class(0, "USB", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_USB, "hid_task", c_hid_task);
  mrbc_define_method(0, mrbc_class_USB, "merge_keyboard_report", c_merge_keyboard_report);
  mrbc_define_method(0, mrbc_class_USB, "merge_consumer_report", c_merge_consumer_report);
  mrbc_define_method(0, mrbc_class_USB, "merge_mouse_report", c_merge_mouse_report);
  mrbc_define_method(0, mrbc_class_USB, "report_raw_hid", c_report_raw_hid);
  mrbc_define_method(0, mrbc_class_USB, "raw_hid_report_received?", c_raw_hid_report_received_q);
  mrbc_define_method(0, mrbc_class_USB, "get_last_received_raw_hid_report", c_get_last_received_raw_hid_report);
  mrbc_define_method(0, mrbc_class_USB, "start_observing_output_report", c_start_observing_output_report);
  mrbc_define_method(0, mrbc_class_USB, "stop_observing_output_report", c_stop_observing_output_report);
  mrbc_define_method(0, mrbc_class_USB, "output_report", c_output_report);

  memset(&mouse, 0, sizeof(MouseValues));
}
