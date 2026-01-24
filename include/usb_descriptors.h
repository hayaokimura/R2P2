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

void USB_hid_init(void);

#endif /* USB_DESCRIPTORS_H_ */
