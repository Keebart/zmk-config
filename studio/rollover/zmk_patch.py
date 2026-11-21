"""Generate Studio-only dual-report HID sources from the pinned ZMK checkout."""
from pathlib import Path
import sys

def once(text, old, new):
    if text.count(old)!=1:
        raise RuntimeError(f'Pinned ZMK rollover anchor changed: {old[:100]!r} ({text.count(old)})')
    return text.replace(old,new)

def patch(kind, text):
    if kind=='nrfx':
        # ZMK_USB_BOOT selects USB_DEVICE_SOF, but neither Studio HID interface
        # installs an on_idle callback and neither uses isochronous endpoints.
        # Queuing 1,000 unused frame events/s can overflow the nRF USB event
        # queue during enumeration. Its reinit path then disables USBD without
        # another cable-detected event to bring it back. Boot/report protocol
        # switching itself needs no SOF interrupts.
        return once(text,'nrfx_usbd_start(IS_ENABLED(CONFIG_USB_DEVICE_SOF));',
                    'nrfx_usbd_start(false); /* Studio HID has no SOF consumer. */')
    if kind=='header':
        # ID 1 remains the existing six-key/LED report, IDs 2 and 3 unchanged.
        # Append a separate full-usage keyboard report; normal/boot typing stays 6KRO.
        anchor='    HID_END_COLLECTION,\n    HID_USAGE_PAGE(HID_USAGE_CONSUMER),'
        descriptor='''    HID_END_COLLECTION,
    HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP), HID_USAGE(HID_USAGE_GD_KEYBOARD),
    HID_COLLECTION(HID_COLLECTION_APPLICATION), HID_REPORT_ID(4),
    HID_USAGE_PAGE(HID_USAGE_KEY),
    HID_USAGE_MIN8(0xe0), HID_USAGE_MAX8(0xe7),
    HID_LOGICAL_MIN8(0), HID_LOGICAL_MAX8(1), HID_REPORT_SIZE(1), HID_REPORT_COUNT(8),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
    HID_USAGE_MIN8(0), HID_USAGE_MAX8(239), HID_REPORT_SIZE(1), HID_REPORT_COUNT(240),
    HID_INPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    HID_USAGE_PAGE(HID_USAGE_LED), HID_USAGE_MIN8(1), HID_USAGE_MAX8(5),
    HID_REPORT_SIZE(1), HID_REPORT_COUNT(5),
    HID_OUTPUT(ZMK_HID_MAIN_VAL_DATA | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
    HID_REPORT_SIZE(3), HID_REPORT_COUNT(1),
    HID_OUTPUT(ZMK_HID_MAIN_VAL_CONST | ZMK_HID_MAIN_VAL_VAR | ZMK_HID_MAIN_VAL_ABS),
#endif
    HID_END_COLLECTION,
    HID_USAGE_PAGE(HID_USAGE_CONSUMER),'''
        return once(text,anchor,descriptor)
    if kind=='hid':
        text=once(text,'static int explicit_button_counts[5] = {0, 0, 0, 0, 0};','static int explicit_button_counts[ZMK_HID_MOUSE_NUM_BUTTONS] = {0};')
        text=once(text,'#include <zmk/hid.h>','#include <zmk/hid.h>\n#include "rollover/zmk.h"\nstatic struct ksn_mode ks_rollover;\nstatic uint8_t ks_nkro_bits[KS_NKRO_BYTES];\nstatic bool ks_nkro_track(zmk_key_t,bool);\nstatic bool ks_nkro_held(zmk_key_t);\nBUILD_ASSERT(CONFIG_ZMK_HID_KEYBOARD_REPORT_SIZE == 6 && IS_ENABLED(CONFIG_ZMK_HID_REPORT_TYPE_HKRO));')
        text=once(text,'    TOGGLE_KEYBOARD(0U, usage);','    if(!ks_nkro_track(usage,true))return 0;\n    TOGGLE_KEYBOARD(0U, usage);')
        text=once(text,'    TOGGLE_KEYBOARD(usage, 0U);','    if(!ks_nkro_track(usage,false))return 0;\n    TOGGLE_KEYBOARD(usage, 0U);')
        start=text.index('static inline int check_keyboard_usage(zmk_key_t usage) {')
        end=text.index('\n#else\n#error "A proper HID report type',start)
        text=text[:start]+'static inline int check_keyboard_usage(zmk_key_t usage) { return ks_nkro_held(usage); }\n'+text[end:]
        text=once(text,'void zmk_hid_keyboard_clear(void) {','void zmk_hid_keyboard_clear(void) {\n    memset(ks_nkro_bits,0,sizeof(ks_nkro_bits));\n#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)\n    keys_held=0;\n#endif')
        return text+'\n#include "rollover/zmk_hid.inc"\n'
    if kind=='usb':
        text=once(text,'#include <zmk/hid.h>','#include <zmk/hid.h>\n#include "rollover/zmk.h"')
        # Regular sends select mode, while GET_REPORT must honor the requested report ID.
        text=once(text,'    struct zmk_hid_keyboard_report *report = zmk_hid_get_keyboard_report();\n    *len = sizeof(*report);\n    return (uint8_t *)report;','    static struct ks_keyboard_packet packet;\n    ks_nkro_snapshot(&packet);\n    *len=packet.len;\n    return packet.data;')
        old='''        case ZMK_HID_REPORT_ID_KEYBOARD: {
            size_t size;
            *data = get_keyboard_report(&size);
            *len = (int32_t)size;
            break;
        }'''
        new='''        case 0:
#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
            if (hid_protocol == HID_PROTOCOL_BOOT) {
                *data=(uint8_t *)zmk_hid_get_boot_report();
                *len=sizeof(zmk_hid_boot_report_t);
                break;
            }
#endif
            return -EINVAL;
        case KS_NKRO_REPORT_ID:
            *data=(uint8_t *)ks_nkro_get_report();
            *len=sizeof(struct ks_nkro_report);
            break;
        case ZMK_HID_REPORT_ID_KEYBOARD: {
#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
            if (hid_protocol == HID_PROTOCOL_BOOT) {
                *data=(uint8_t *)zmk_hid_get_boot_report();
                *len=sizeof(zmk_hid_boot_report_t);
                break;
            }
#endif
            static struct zmk_hid_keyboard_report report;
            report=*zmk_hid_get_keyboard_report();
            if(ks_nkro_enabled())memset(&report.body,0,sizeof(report.body));
            *data=(uint8_t *)&report; *len=sizeof(report);
            break;
        }'''
        text=once(text,old,new)
        text=once(text,'        case ZMK_HID_REPORT_ID_LEDS:', '''        case 0:
#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
            if(hid_protocol==HID_PROTOCOL_BOOT && *len==sizeof(struct zmk_hid_led_report_body)) {
                struct zmk_endpoint_instance endpoint={.transport=ZMK_TRANSPORT_USB};
                zmk_hid_indicators_process_report((struct zmk_hid_led_report_body *)*data,endpoint);
                break;
            }
#endif
            return -EINVAL;
        case KS_NKRO_REPORT_ID:
        case ZMK_HID_REPORT_ID_LEDS:''')
        text=once(text,'    return zmk_usb_hid_send_report(report, len);','    bool awake=zmk_usb_get_status()!=USB_DC_SUSPEND;\n    int result=zmk_usb_hid_send_report(report,len);\n    if(awake)ks_nkro_sent(result);\n    return result;')
        return text
    if kind=='hog':
        text=once(text,'#include <zmk/hid.h>','#include <zmk/hid.h>\n#include "rollover/zmk.h"')
        text=once(text,'    struct zmk_hid_keyboard_report_body *report_body = &zmk_hid_get_keyboard_report()->body;', '    struct zmk_hid_keyboard_report_body neutral={0};\n    struct zmk_hid_keyboard_report_body *report_body = ks_nkro_enabled()?&neutral:&zmk_hid_get_keyboard_report()->body;')
        anchor='/* HID Service Declaration */'
        text=once(text,anchor,'''static struct hids_report nkro_input = {.id=KS_NKRO_REPORT_ID,.type=HIDS_INPUT};
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static struct hids_report nkro_leds = {.id=KS_NKRO_REPORT_ID,.type=HIDS_OUTPUT};
#endif
static ssize_t read_nkro(struct bt_conn *conn,const struct bt_gatt_attr *attr,void *buf,uint16_t len,uint16_t offset) {
    struct ks_nkro_report *report=ks_nkro_get_report();
    return bt_gatt_attr_read(conn,attr,buf,len,offset,&report->mods,sizeof(*report)-1);
}
'''+anchor)
        anchor='BT_GATT_PERM_WRITE, NULL, write_ctrl_point, &ctrl_point));'
        text=once(text,anchor,'''BT_GATT_PERM_WRITE, NULL, write_ctrl_point, &ctrl_point),
    // Append to preserve every existing HID attribute handle.
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT, NULL,
                           write_hids_leds_report, NULL),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref,
                       NULL, &nkro_leds),
#endif
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ_ENCRYPT, read_nkro, NULL, NULL),
    BT_GATT_CCC(input_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref,
                       NULL, &nkro_input));''')
        start=text.index('K_MSGQ_DEFINE(zmk_hog_keyboard_msgq,')
        end=text.index('K_MSGQ_DEFINE(zmk_hog_consumer_msgq,',start)
        chunk=text[start:end]
        chunk=chunk.replace('struct zmk_hid_keyboard_report_body report;', 'struct ks_keyboard_packet report;').replace('struct zmk_hid_keyboard_report_body discarded_report;', 'struct ks_keyboard_packet discarded_report;').replace('sizeof(struct zmk_hid_keyboard_report_body)', 'sizeof(struct ks_keyboard_packet)')
        chunk=once(chunk,'.attr = &hog_svc.attrs[5],','.attr = report.data[0]==KS_NKRO_REPORT_ID ? &hog_svc.attrs[hog_svc.attr_count-3] : &hog_svc.attrs[5],')
        chunk=once(chunk,'.data = &report,','.data = report.data+1,')
        chunk=once(chunk,'.len = sizeof(report),','.len = report.len-1,')
        chunk=once(chunk,'    int err = k_msgq_put(&zmk_hog_keyboard_msgq, report, K_MSEC(100));','    (void)report;\n    struct ks_keyboard_packet packet;\n    ks_nkro_snapshot(&packet);\n    int err = k_msgq_put(&zmk_hog_keyboard_msgq, &packet, K_MSEC(100));')
        a=chunk.index('    if (err) {',chunk.index('int zmk_hog_send_keyboard_report'))
        b=chunk.index('    k_work_submit_to_queue',a)
        chunk=chunk[:a]+'    if (err) return err; // Never discard a neutral report at a mode boundary.\n\n'+chunk[b:]
        chunk=once(chunk,'    k_work_submit_to_queue(&hog_work_q, &hog_keyboard_work);','    ks_nkro_sent(0);\n    k_work_submit_to_queue(&hog_work_q, &hog_keyboard_work);')
        return text[:start]+chunk+text[end:]
    raise ValueError(kind)

if __name__=='__main__':
    Path(sys.argv[3]).write_text(patch(sys.argv[1],Path(sys.argv[2]).read_text()),encoding='utf-8')
