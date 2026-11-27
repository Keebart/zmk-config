/* Keebart Studio Sharp MiP peripheral (right) status screen widget.
 * Event-driven, battery-safe renderer showing the peripheral's own battery %,
 * charging bolt, wireless split-link status, and preset artwork.
 */
#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/usb.h>

#include "mip_boot_bitmaps.h"
#include "mip_canvas.h"
#include "peripheral_status.h"

extern bool ks_power_display_dark(void);

#define KSM_PHYS_W 160
#define KSM_PHYS_H 68
#define KSM_SPLASH_DURATION_MS 2500u

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static lv_color_t studio_cbuf[KSM_PHYS_W * KSM_PHYS_H];
static uint8_t studio_fb[KSM_FB_BYTES];
static uint8_t studio_prev_fb[KSM_FB_BYTES];
static bool studio_fb_valid;
static bool studio_prev_dark;
static bool studio_splash_active;
static ksm_state_t studio_state;
static lv_obj_t *studio_canvas;

struct peripheral_status_state {
    bool connected;
};

static void studio_mip_flush(void) {
    if (!studio_canvas) {
        return;
    }
    if (studio_splash_active) {
        memcpy(studio_fb, KSM_BOOT_BITMAP, sizeof(studio_fb));
    } else {
        studio_state.now_ms = (uint32_t)k_uptime_get();
        ksm_preset_render_right(studio_fb, &studio_state);
    }

    const bool dark = ks_power_display_dark();
    if (studio_fb_valid && dark == studio_prev_dark &&
        memcmp(studio_fb, studio_prev_fb, sizeof(studio_fb)) == 0) {
        return;
    }
    memcpy(studio_prev_fb, studio_fb, sizeof(studio_fb));
    studio_fb_valid = true;
    studio_prev_dark = dark;

    /* Sharp LS011B7DH03 via Zephyr PIXEL_FORMAT_MONO01:
     * lv_color_black() (0) drives reflective silver; lv_color_white() (1) drives dark ink.
     * Light mode (default, dark == false): dark ink foreground on reflective silver background.
     * Dark mode (dark == true): reflective silver foreground on dark matte background.
     */
    const lv_color_t fg = dark ? lv_color_black() : lv_color_white();
    const lv_color_t bg = dark ? lv_color_white() : lv_color_black();
    for (int py = 0; py < (int)KSM_HEIGHT; py++) {
        for (int px = 0; px < (int)KSM_WIDTH; px++) {
            uint8_t on = ksm_get_px(studio_fb, px, py);
#ifdef CONFIG_SHARP_MIP_ROTATE_180
            int lx = py;
            int ly = ((int)KSM_WIDTH - 1) - px;
#else
            int lx = ((int)KSM_HEIGHT - 1) - py;
            int ly = px;
#endif
            studio_cbuf[ly * KSM_PHYS_W + lx] = on ? fg : bg;
        }
    }
    lv_obj_invalidate(studio_canvas);
}

static void studio_theme_work_fn(struct k_work *work) {
    (void)work;
    studio_mip_flush();
}

static K_WORK_DEFINE(studio_theme_work, studio_theme_work_fn);

void ks_mip_theme_changed(void) {
    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &studio_theme_work);
    }
}

static void studio_splash_end_fn(struct k_work *work) {
    (void)work;
    if (!studio_splash_active) {
        return;
    }
    studio_splash_active = false;
    studio_mip_flush();
}

static K_WORK_DELAYABLE_DEFINE(studio_splash_work, studio_splash_end_fn);

static void battery_status_update_cb(struct battery_status_state st) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    studio_state.peripheral_charging = st.usb_present ? 1u : 0u;
#endif
    studio_state.peripheral_battery = st.level <= 100u ? st.level : KSM_BATTERY_UNKNOWN;
    studio_mip_flush();
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    (void)eh;
    return (struct battery_status_state){
        .level = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

static struct peripheral_status_state get_state(const zmk_event_t *eh) {
    (void)eh;
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected()};
}

static void output_status_update_cb(struct peripheral_status_state st) {
    studio_state.peripheral_connected = st.connected ? 1u : 0u;
    studio_mip_flush();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    memset(&studio_state, 0, sizeof(studio_state));
    studio_fb_valid = false;
    studio_prev_dark = false;
    studio_splash_active = true;
    studio_state.peripheral_battery = zmk_battery_state_of_charge();
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    studio_state.peripheral_charging = zmk_usb_is_powered() ? 1u : 0u;
#endif
    studio_state.peripheral_connected = zmk_split_bt_peripheral_is_connected() ? 1u : 0u;

    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, KSM_PHYS_W, KSM_PHYS_H);

    studio_canvas = lv_canvas_create(widget->obj);
    lv_obj_align(studio_canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(studio_canvas, studio_cbuf, KSM_PHYS_W, KSM_PHYS_H, LV_IMG_CF_TRUE_COLOR);

    sys_slist_append(&widgets, &widget->node);
    studio_mip_flush();
    k_work_reschedule_for_queue(zmk_display_work_q(), &studio_splash_work,
                                K_MSEC(KSM_SPLASH_DURATION_MS));

    widget_battery_status_init();
    widget_peripheral_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
