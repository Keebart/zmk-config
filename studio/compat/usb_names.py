#!/usr/bin/env python3
# Studio-only copy of the pinned USB source. Passive OS observations never
# consume requests or modify any descriptors, bcdUSB, interfaces, or BOS support.
from pathlib import Path
import sys
source=Path(sys.argv[1]).read_text()
needle='\tLOG_DBG("Get Descriptor request");'
assert source.count(needle)==1,'Pinned USB descriptor handler changed'
hook='\textern void keebart_os_descriptor(uint16_t length);\n\tif (USB_GET_DESCRIPTOR_TYPE(setup->wValue) == USB_DESC_STRING)\n\t\tkeebart_os_descriptor(setup->wLength);\n'
hook+='\textern const uint8_t *keebart_usb_name(void);\n\tif (USB_GET_DESCRIPTOR_TYPE(setup->wValue) == USB_DESC_STRING &&\n\t    USB_GET_DESCRIPTOR_INDEX(setup->wValue) == 2) {\n\t\tconst uint8_t *name = keebart_usb_name();\n\t\tif (name) { *len = name[0]; *data = (uint8_t *)name; return true; }\n\t}\n'
source=source.replace(needle,hook+needle)
needle='static void forward_status_cb(enum usb_dc_status_code status, const uint8_t *param)\n{'
assert source.count(needle)==1,'Pinned USB status callback changed'
source=source.replace(needle,needle+'\n\textern void keebart_os_usb(bool configured);\n\tif (status == USB_DC_RESET || status == USB_DC_DISCONNECTED) keebart_os_usb(false);\n\telse if (status == USB_DC_CONFIGURED) keebart_os_usb(true);')
Path(sys.argv[2]).write_text(source)
