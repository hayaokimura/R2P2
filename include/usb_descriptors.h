#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

#include "tusb.h"

enum {
  REPORT_ID_KEYBOARD = 1,
  REPORT_ID_MOUSE,
  REPORT_ID_CONSUMER_CONTROL,
  REPORT_ID_RAWHID,
  REPORT_ID_COUNT
};

// HID Config flags
#define HID_CONFIG_KEYBOARD    (1 << 0)
#define HID_CONFIG_CONSUMER    (1 << 1)
#define HID_CONFIG_MOUSE       (1 << 2)
#define HID_CONFIG_RAWHID      (1 << 3)
#define HID_CONFIG_DEFAULT     0x0F  // All enabled

// HID config functions
void hid_config_load(void);
uint8_t hid_config_get_flags(void);
bool hid_config_save(uint8_t flags);
void build_hid_report_descriptors(void);

void USB_hid_init(void);

#endif /* USB_DESCRIPTORS_H_ */
