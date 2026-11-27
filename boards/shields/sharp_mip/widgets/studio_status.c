/* Keebart Studio Sharp MiP central (left) status screen widget.
 * Event-driven, battery-safe renderer that feeds live ZMK wireless & Studio
 * telemetry into the active 68x160 monochrome Sharp MiP preset.
 */
#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/hid_indicators.h>
#endif
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>
#include <zmk/wpm.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
#include <zmk/split/central.h>
#endif

#include "display_event.h"
#include "mip_boot_bitmaps.h"
#include "mip_canvas.h"
#include "status.h"

extern void ks_display_layer_name(unsigned layer, char *out, size_t size);
extern bool ks_display_last_key(char *out, size_t size);
extern bool is_caps_word_on(void);
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

static uint8_t pack_mods(zmk_mod_flags_t raw) {
    uint8_t m = 0;
    if (raw & (MOD_LCTL | MOD_RCTL)) {
        m |= KSM_MOD_CTRL;
    }
    if (raw & (MOD_LSFT | MOD_RSFT)) {
        m |= KSM_MOD_SHIFT;
    }
    if (raw & (MOD_LALT | MOD_RALT)) {
        m |= KSM_MOD_ALT;
    }
    if (raw & (MOD_LGUI | MOD_RGUI)) {
        m |= KSM_MOD_GUI;
    }
    return m;
}

static uint8_t read_locks(void) {
    uint8_t l = 0;
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    zmk_hid_indicators_t ind = zmk_hid_indicators_get_current_profile();
    if (ind & BIT(0)) {
        l |= KSM_LOCK_NUM;
    }
    if (ind & BIT(1)) {
        l |= KSM_LOCK_CAPS;
    }
    if (ind & BIT(2)) {
        l |= KSM_LOCK_SCROLL;
    }
#endif
    if (is_caps_word_on()) {
        l |= KSM_LOCK_CAPS_WORD;
    }
    return l;
}

static void copy_ascii_bounded(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) {
        return;
    }
    size_t n = 0;
    if (src) {
        while (src[n] && n + 1 < dst_size) {
            unsigned char c = (unsigned char)src[n];
            dst[n] = (c >= 32u && c <= 126u) ? (char)c : ' ';
            n++;
        }
    }
    dst[n] = '\0';
}

/* Blit 68x160 portrait framebuffer onto 160x68 physical Sharp MiP canvas.
 * Matches legacy sharp_mip rotation:
 *  - Default (Corne BT, Piantor BT): +90 deg CW  -> phys(159 - py, px)
 *  - CONFIG_SHARP_MIP_ROTATE_180 (Sofle BT): -90 deg CCW -> phys(py, 67 - px)
 *
 * Sharp LS011B7DH03 via Zephyr PIXEL_FORMAT_MONO01:
 *  - lv_color_black() (0) drives reflective silver pixels
 *  - lv_color_white() (1) drives dark absorbing ink pixels
 * Light mode (default, dark == false): dark ink foreground on reflective silver background.
 * Dark mode (dark == true): reflective silver foreground on dark matte background.
 */
static void studio_mip_flush(void) {
    if (!studio_canvas) {
        return;
    }
    if (studio_splash_active) {
        memcpy(studio_fb, KSM_BOOT_BITMAP, sizeof(studio_fb));
    } else {
        studio_state.now_ms = (uint32_t)k_uptime_get();
        ksm_preset_render_left(studio_fb, &studio_state);
    }

    const bool dark = ks_power_display_dark();
    if (studio_fb_valid && dark == studio_prev_dark &&
        memcmp(studio_fb, studio_prev_fb, sizeof(studio_fb)) == 0) {
        return;
    }
    memcpy(studio_prev_fb, studio_fb, sizeof(studio_fb));
    studio_fb_valid = true;
    studio_prev_dark = dark;

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

static void refresh_output_fields(void) {
    struct zmk_endpoint_instance ep = zmk_endpoints_selected();
    studio_state.transport =
        (ep.transport == ZMK_TRANSPORT_USB) ? KSM_TRANSPORT_USB : KSM_TRANSPORT_BLE;
#if IS_ENABLED(CONFIG_ZMK_BLE)
    int active = zmk_ble_active_profile_index();
    studio_state.active_profile =
        (active >= 0 && active < (int)KSM_PROFILE_COUNT) ? (uint8_t)active : 0u;
    uint8_t bonded = 0;
    uint8_t connected = 0;
    uint8_t limit = (uint8_t)KSM_PROFILE_COUNT;
    if ((uint8_t)ZMK_BLE_PROFILE_COUNT < limit) {
        limit = (uint8_t)ZMK_BLE_PROFILE_COUNT;
    }
    for (uint8_t i = 0; i < limit; i++) {
        if (!zmk_ble_profile_is_open(i)) {
            bonded |= (uint8_t)(1u << i);
        }
        if (zmk_ble_profile_is_connected(i)) {
            connected |= (uint8_t)(1u << i);
        }
    }
    studio_state.profiles_bonded = bonded;
    studio_state.profiles_connected = connected;
#else
    studio_state.active_profile = 0u;
    studio_state.profiles_bonded = 0u;
    studio_state.profiles_connected = 0u;
#endif
}

static void refresh_layer_fields(void) {
    zmk_keymap_layer_index_t idx = zmk_keymap_highest_layer_active();
    studio_state.layer = (uint8_t)idx;
    char label[129];
    label[0] = '\0';
    ks_display_layer_name((unsigned)idx, label, sizeof(label));
    if (!label[0]) {
        const char *fallback = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(idx));
        if (fallback) {
            copy_ascii_bounded(studio_state.layer_name, sizeof(studio_state.layer_name), fallback);
            return;
        }
    }
    copy_ascii_bounded(studio_state.layer_name, sizeof(studio_state.layer_name), label);
}

static void refresh_peripheral_battery(void) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
    uint8_t level = 0;
    if (zmk_split_central_get_peripheral_battery_level(0, &level) == 0 && level > 0u &&
        level <= 100u) {
        studio_state.peripheral_connected = 1u;
        studio_state.peripheral_battery = level;
    } else {
        studio_state.peripheral_connected = 0u;
        studio_state.peripheral_battery = KSM_BATTERY_UNKNOWN;
    }
#else
    studio_state.peripheral_connected = 0u;
    studio_state.peripheral_battery = KSM_BATTERY_UNKNOWN;
#endif
}

/* --- Battery & USB power listener --- */
static void battery_update_cb(struct battery_status_state st) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    studio_state.central_charging = st.usb_present ? 1u : 0u;
#endif
    studio_state.central_battery = st.level <= 100u ? st.level : KSM_BATTERY_UNKNOWN;
    studio_mip_flush();
}

static struct battery_status_state battery_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state, battery_update_cb,
                            battery_get_state)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

/* --- Peripheral battery listener --- */
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
struct periph_batt_state {
    uint8_t level;
};

static void periph_batt_update_cb(struct periph_batt_state st) {
    if (st.level > 0u && st.level <= 100u) {
        studio_state.peripheral_connected = 1u;
        studio_state.peripheral_battery = st.level;
    } else {
        studio_state.peripheral_connected = 0u;
        studio_state.peripheral_battery = KSM_BATTERY_UNKNOWN;
    }
    studio_mip_flush();
}

static struct periph_batt_state periph_batt_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (ev && ev->source == 0u) {
        return (struct periph_batt_state){.level = ev->state_of_charge};
    }
    uint8_t level = 0;
    if (zmk_split_central_get_peripheral_battery_level(0, &level) != 0) {
        level = 0;
    }
    return (struct periph_batt_state){.level = level};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_periph_battery_status, struct periph_batt_state,
                            periph_batt_update_cb, periph_batt_get_state)
ZMK_SUBSCRIPTION(widget_periph_battery_status, zmk_peripheral_battery_state_changed);
#endif

/* --- Output & BT profile listener --- */
struct output_trigger_state {
    uint8_t dummy;
};

static void output_update_cb(struct output_trigger_state st) {
    (void)st;
    refresh_output_fields();
    studio_mip_flush();
}

static struct output_trigger_state output_get_state(const zmk_event_t *eh) {
    (void)eh;
    return (struct output_trigger_state){0};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_trigger_state, output_update_cb,
                            output_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

/* --- Layer listener --- */
struct layer_trigger_state {
    uint8_t dummy;
};

static void layer_update_cb(struct layer_trigger_state st) {
    (void)st;
    refresh_layer_fields();
    studio_mip_flush();
}

static struct layer_trigger_state layer_get_state(const zmk_event_t *eh) {
    (void)eh;
    return (struct layer_trigger_state){0};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_trigger_state, layer_update_cb,
                            layer_get_state)
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

/* --- WPM listener --- */
struct wpm_status_state {
    uint8_t wpm;
};

static void wpm_update_cb(struct wpm_status_state st) {
    for (int i = 0; i < (int)KSM_WPM_SAMPLES - 1; i++) {
        studio_state.wpm_history[i] = studio_state.wpm_history[i + 1];
    }
    studio_state.wpm_history[KSM_WPM_SAMPLES - 1] = st.wpm;
    studio_state.wpm = st.wpm;
    studio_mip_flush();
}

static struct wpm_status_state wpm_get_state(const zmk_event_t *eh) {
    (void)eh;
    return (struct wpm_status_state){.wpm = zmk_wpm_get_state()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state, wpm_update_cb,
                            wpm_get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

/* --- Modifiers, Caps Word, HID lock indicators & Studio last key --- */
static void studio_input_work_fn(struct k_work *work) {
    (void)work;
    char text[192];
    if (ks_display_last_key(text, sizeof(text))) {
        copy_ascii_bounded(studio_state.last_key, sizeof(studio_state.last_key), text);
    }
    studio_state.mods = pack_mods(zmk_hid_get_explicit_mods());
    studio_state.locks = read_locks();
    studio_mip_flush();
}

static K_WORK_DEFINE(studio_input_work, studio_input_work_fn);

static int studio_input_ev_cb(const zmk_event_t *eh) {
    (void)eh;
    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &studio_input_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

/* ks_display_key_changed is raised for Studio last-key updates as well as
 * explicit modifier press/release and Caps Word transitions from override_hid.c.
 */
ZMK_LISTENER(widget_studio_input, studio_input_ev_cb);
ZMK_SUBSCRIPTION(widget_studio_input, ks_display_key_changed);
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
ZMK_SUBSCRIPTION(widget_studio_input, zmk_hid_indicators_changed);
#endif

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    memset(&studio_state, 0, sizeof(studio_state));
    studio_fb_valid = false;
    studio_prev_dark = false;
    studio_splash_active = true;
    studio_state.central_battery = zmk_battery_state_of_charge();
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    studio_state.central_charging = zmk_usb_is_powered() ? 1u : 0u;
#endif
    refresh_peripheral_battery();
    refresh_output_fields();
    refresh_layer_fields();
    studio_state.mods = pack_mods(zmk_hid_get_explicit_mods());
    studio_state.locks = read_locks();

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
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
    widget_periph_battery_status_init();
#endif
    widget_output_status_init();
    widget_layer_status_init();
    widget_wpm_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
