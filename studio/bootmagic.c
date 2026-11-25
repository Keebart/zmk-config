#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/usb/usb_device.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

#include "bootmagic.h"
#include "hardware.h"

#define KS_BOOTLOADER_MAGIC 0x57

static struct ks_bootmagic bootmagic = {
#if IS_ENABLED(CONFIG_KEEBART_STUDIO_PERIPHERAL)
    .magic = KS_BOOTMAGIC_RIGHT,
#else
    .magic = KS_BOOTMAGIC_LEFT,
#endif
};

static void bootmagic_usb_flags(bool *vbus, bool *host) {
    enum usb_dc_status_code st = zmk_usb_get_status();
    *vbus = (st != USB_DC_DISCONNECTED && st != USB_DC_UNKNOWN);
    *host = (st == USB_DC_CONFIGURED || st == USB_DC_RESET || zmk_usb_is_hid_ready());
}

static int bootmagic_init(void) {
    bool vbus = false;
    bool host = false;
    bootmagic_usb_flags(&vbus, &host);
    unsigned int key = irq_lock();
    bool trig = ks_bootmagic_boot_ready(&bootmagic, k_uptime_get_32(), vbus, host);
    irq_unlock(key);
    if (trig) {
        sys_reboot(KS_BOOTLOADER_MAGIC);
    }
    return 0;
}

static int bootmagic_event(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *pos = as_zmk_position_state_changed(eh);
    if (pos) {
        if (pos->source != ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL) {
            return 0;
        }
        unsigned int key = irq_lock();
        bool trig = ks_bootmagic_key(&bootmagic, pos->position, pos->state, k_uptime_get_32());
        irq_unlock(key);
        if (trig) {
            sys_reboot(KS_BOOTLOADER_MAGIC);
        }
        return 0;
    }
    if (as_zmk_usb_conn_state_changed(eh)) {
        bool vbus = false;
        bool host = false;
        bootmagic_usb_flags(&vbus, &host);
        unsigned int key = irq_lock();
        bool trig = ks_bootmagic_usb(&bootmagic, vbus, host);
        irq_unlock(key);
        if (trig) {
            sys_reboot(KS_BOOTLOADER_MAGIC);
        }
    }
    return 0;
}

SYS_INIT(bootmagic_init, APPLICATION, 99);
ZMK_LISTENER(keebart_bootmagic, bootmagic_event);
ZMK_SUBSCRIPTION(keebart_bootmagic, zmk_position_state_changed);
ZMK_SUBSCRIPTION(keebart_bootmagic, zmk_usb_conn_state_changed);
