#include "consumer_keys.h"
#define DT_DRV_COMPAT keebart_studio_key
#include "core.h"
void ksn_task(void);
bool ksn_reset(void);
void ksn_handle(const uint8_t[32],uint8_t[32],bool);
#include "stock.h"
#include "store.h"
#include "combo.h"
#include "render.h"
#include "runtime.h"
#include <zmk/virtual_key_position.h>
#include <zmk/keymap.h>
#include <zmk/events/sensor_event.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/activity_state_changed.h>
#include <limits.h>
#include <drivers/behavior.h>
#include <dt-bindings/zmk/bt.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zmk/behavior.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>
extern int keebart_storage_error(void);
static struct ks_state state;
const struct ks_override *ks_override_entries(void){return state.active.overrides;}
extern void ks_override_lifecycle_cancel(void);
static struct ks_store storage;
static struct ks_combo_engine combos;
static struct kd_engine dances;
static void dance_emit(void *, unsigned, uint16_t, enum kd_action);
static void dance_tick(struct k_work *);
K_WORK_DELAYABLE_DEFINE(dance_work,dance_tick);
static struct ks_renderer renderer;
static uint8_t host_locks, base_layer;
static uint16_t locked_layers;
// Queue complete taps, never separate press/release records. A full queue drops
// a whole detent, so encoder bursts cannot strand a pressed key.
#if KS_ENCODERS
struct encoder_tap {
    struct zmk_behavior_binding_event event;
    uint16_t code;
};
K_MSGQ_DEFINE(encoder_taps, sizeof(struct encoder_tap), 64, 4);
#endif
static unsigned pending_encoder_taps;
static bool lighting_enabled = true;
static int execute(uint16_t, struct zmk_behavior_binding_event, bool);
static struct km_vm macro_vm;
static void macro_emit(void *, uint16_t, bool);
static void macro_run(struct k_work *);
K_WORK_DELAYABLE_DEFINE(macro_work, macro_run);
static void combo_emit(void *, unsigned, uint16_t, bool, int64_t);
static void combo_timeout(struct k_work *);
static void save_lighting(struct k_work *);
K_WORK_DELAYABLE_DEFINE(lighting_save_work, save_lighting);
K_WORK_DELAYABLE_DEFINE(combo_work, combo_timeout);
// A save may finish between a physical release event and behavior dispatch.
// Resolve releases from the same binding as their press, including transparent
// layers.
static uint16_t pressed_codes[KS_LAYERS][KS_KEYS];
K_MUTEX_DEFINE(state_mutex);
K_MSGQ_DEFINE(requests, 32, 2, 4);
K_SEM_DEFINE(tx_ready, 1, 1);
static const struct device *hid;
static int write_record(const char *name, const void *data, size_t len) {
    return settings_save_one(name, data, len);
}
static int persist(const struct ks_config *config) {
    return ks_store_save(&storage, config, write_record);
}
static void save_lighting(struct k_work *work) {
    k_mutex_lock(&state_mutex, K_FOREVER);
    if (state.staging || state.pressed)
        k_work_reschedule(&lighting_save_work, K_MSEC(1000));
    else if (persist(&state.active))
        k_work_reschedule(&lighting_save_work, K_MSEC(5000));
    k_mutex_unlock(&state_mutex);
}
static int settings_read(const char *name, size_t len, settings_read_cb read, void *arg) {
    static uint8_t buffer[KS_STORE_CHUNK];
    if (len > sizeof(buffer) || read(arg, buffer, len) != len)
        return -EINVAL;
    return ks_store_receive(&storage, name, buffer, len) ? 0 : -EINVAL;
}
static void locale_load(void);
static void locale_endpoint(void);
static void locale_cancel(void);
static void locale_schedule(void);
static int keebart_settings_commit(void) {
    ks_store_load(&storage, &state);
    locale_load();
    base_layer = state.active.default_layer;
    if (base_layer)
        zmk_keymap_layer_activate(base_layer);
    return 0;
}
SETTINGS_STATIC_HANDLER_DEFINE(keebart, "keebart", NULL, settings_read, keebart_settings_commit,
                               NULL);
static int observe(const zmk_event_t *event) {
    const struct zmk_position_state_changed *pos = as_zmk_position_state_changed(event);
    k_mutex_lock(&state_mutex, K_FOREVER);
    if (pos) {
        ks_position(&state, pos->position, pos->state, pos->timestamp);
        locale_schedule();
        if (pos->state)
            ks_render_press(&renderer, pos->position, pos->timestamp);

#if KS_ENCODERS
    } else if (as_zmk_sensor_event(event)) {
        const struct zmk_sensor_event *e = as_zmk_sensor_event(event);
        if (e->sensor_index < KS_ENCODERS && e->channel_data_size == 1) {
            const struct zmk_sensor_config *cfg = zmk_sensors_get_config_at_index(e->sensor_index);
            ks_rotate(&state, e->sensor_index, e->channel_data[0].value.val1,
                      e->channel_data[0].value.val2, cfg->triggers_per_rotation);
        }

#endif
    } else if (as_zmk_layer_state_changed(event)) {
        const struct zmk_layer_state_changed *e = as_zmk_layer_state_changed(event);
        if (!e->state && ((locked_layers & BIT(e->layer)) || e->layer == base_layer))
            zmk_keymap_layer_activate(e->layer);
    } else if (as_zmk_hid_indicators_changed(event)) {
        host_locks = as_zmk_hid_indicators_changed(event)->indicators;
    } else if (as_zmk_endpoint_changed(event) || as_zmk_ble_active_profile_changed(event) || (as_zmk_activity_state_changed(event) && as_zmk_activity_state_changed(event)->state == ZMK_ACTIVITY_SLEEP)) {
        locale_endpoint();
        ks_override_lifecycle_cancel();
        kd_cancel(&dances,dance_emit,NULL);
        km_cancel(&macro_vm, macro_emit, NULL);
    } else if (as_zmk_usb_conn_state_changed(event)) {
        locale_endpoint();
        ks_override_lifecycle_cancel();
        kd_cancel(&dances,dance_emit,NULL);
        km_cancel(&macro_vm, macro_emit, NULL);
        ks_lock(&state);
        k_msgq_purge(&requests);
    }
    k_mutex_unlock(&state_mutex);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(keebart_observe, observe);
ZMK_SUBSCRIPTION(keebart_observe, zmk_position_state_changed);
ZMK_SUBSCRIPTION(keebart_observe, zmk_usb_conn_state_changed);
ZMK_SUBSCRIPTION(keebart_observe, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(keebart_observe, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(keebart_observe, zmk_activity_state_changed);
#if KS_ENCODERS
ZMK_SUBSCRIPTION(keebart_observe, zmk_sensor_event);
#endif
ZMK_SUBSCRIPTION(keebart_observe, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(keebart_observe, zmk_hid_indicators_changed);
static uint32_t key(uint16_t code) { return ks_key_usage(code); }
static uint8_t mods(uint16_t code) {
    uint8_t m = (code >> 8) & 15;
    return code & 0x1000 ? m << 4 : m;
}
static uint32_t mod_key(uint8_t m) {
    if (!m)
        return 0;
    unsigned i = 0;
    while (!(m & (1 << i)))
        i++;
    return key(0xe0 + i) | ((uint32_t)(m & ~(1 << i)) << 24);
}
#define NAME(label) DEVICE_DT_NAME(DT_NODELABEL(label))
static struct zmk_behavior_binding translate(uint16_t c) {
    struct zmk_behavior_binding b = {.behavior_dev = NAME(kp), .param1 = key(c)};
    if (c == 0)
        b.behavior_dev = NAME(none);
    else if (c == 1)
        b.behavior_dev = NAME(trans);
    else if (c < 0x100) {
    } else if (c < 0x2000)
        b.param1 = key(c & 255) | ((uint32_t)mods(c) << 24);
    else if (c < 0x4000) {
        b.behavior_dev = NAME(mt);
        b.param1 = mod_key(mods(c));
        b.param2 = key(c & 255);
    } else if (c < 0x5000) {
        b.behavior_dev = NAME(lt);
        b.param1 = (c >> 8) & 15;
        b.param2 = key(c & 255);
    } else if ((c & 0xfff0) == 0x5200) {
        b.behavior_dev = NAME(to);
        b.param1 = c & 15;
    } else if ((c & 0xfff0) == 0x5220) {
        b.behavior_dev = NAME(mo);
        b.param1 = c & 15;
    } else if ((c & 0xfff0) == 0x5260) {
        b.behavior_dev = NAME(tog);
        b.param1 = c & 15;
    } else if ((c & 0xfff0) == 0x5280) {
        b.behavior_dev = NAME(sl);
        b.param1 = c & 15;
    } else if (c == 0x7c00 || c == 0x7c01) {
        b.behavior_dev = c == 0x7c00 ? NAME(bootloader) : NAME(sys_reset);
        b.param1 = 0;
    } else if (c >= 0x7e00 && c <= 0x7e04) {
        b.behavior_dev = NAME(bt);
        b.param1 = BT_SEL_CMD;
        b.param2 = c - 0x7e00;
    } else if (c >= 0x7e10 && c <= 0x7e13) {
        b.behavior_dev = NAME(bt);
        b.param1 = c - 0x7e10;
    } else if (c >= 0x7e20 && c <= 0x7e22) {
        b.behavior_dev = NAME(rgb_ug);
        b.param1 = c == 0x7e20 ? 0 : c == 0x7e21 ? 3 : 4;
    }
    return b;
}
#include "locale_port.inc"
#include "repeat_port.inc"
static int invoke(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event,
                  bool pressed) {
    unsigned l = binding->param1, pos = binding->param2;
    if (l >= KS_LAYERS || pos >= KS_KEYS)
        return -EINVAL;
    k_mutex_lock(&state_mutex, K_FOREVER);
    if (pressed) {
        kd_input(&dances,pos,0,true,event.timestamp,state.active.dances,KD_COUNT,dance_emit,NULL);
        uint16_t code = ksl_binding(state.active.locales,locale_runtime.active,ks_binding(&state,l,pos));
        pressed_codes[l][pos] = code;
    }
    uint16_t code = pressed_codes[l][pos];
    if (code == 1) {
        k_mutex_unlock(&state_mutex);
        return ZMK_BEHAVIOR_TRANSPARENT;
    }
    ks_combo_input(&combos, &state.active, pos, code, pressed, event.timestamp, combo_emit, NULL);
    int64_t due = ks_combo_deadline(&combos, &state.active);
    if (due != INT64_MAX)
        k_work_reschedule(&combo_work, K_MSEC(MAX(1, due - k_uptime_get())));
    k_mutex_unlock(&state_mutex);
    return ZMK_BEHAVIOR_OPAQUE;
}
static void combo_emit(void *arg, unsigned pos, uint16_t code, bool down, int64_t at) {
    struct zmk_behavior_binding_event e = {.position = pos,
                                           .timestamp = at,
                                           .layer = zmk_keymap_highest_layer_active(),
                                           .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL};
    execute(code, e, down);
}
static void combo_timeout(struct k_work *work) {
    k_mutex_lock(&state_mutex, K_FOREVER);
    ks_combo_tick(&combos, &state.active, k_uptime_get(), combo_emit, NULL);
    k_mutex_unlock(&state_mutex);
}
static int pressed(struct zmk_behavior_binding *b, struct zmk_behavior_binding_event e) {
    return invoke(b, e, true);
}
static int released(struct zmk_behavior_binding *b, struct zmk_behavior_binding_event e) {
    return invoke(b, e, false);
}
#if KS_ENCODERS
static int sensor_accept(struct zmk_behavior_binding *b, struct zmk_behavior_binding_event e,
                         const struct zmk_sensor_config *cfg, size_t n,
                         const struct zmk_sensor_channel_data *data) {
    return b->param1 < KS_LAYERS && b->param2 < KS_ENCODERS && n == 1 ? 0 : -EINVAL;
}
static int sensor_process(struct zmk_behavior_binding *b, struct zmk_behavior_binding_event e,
                          enum behavior_sensor_binding_process_mode mode) {
    if (mode != BEHAVIOR_SENSOR_BINDING_PROCESS_MODE_TRIGGER)
        return ZMK_BEHAVIOR_TRANSPARENT;
    k_mutex_lock(&state_mutex, K_FOREVER);
    int steps = state.rotation_steps[b->param2];
    unsigned layer = b->param1;
    uint16_t code =
        state.active.keys[KS_ENCODER_AT + layer * KS_ENCODERS * 2 + b->param2 * 2 + (steps < 0)];
    if (!steps || code == 1) {
        k_mutex_unlock(&state_mutex);
        return ZMK_BEHAVIOR_TRANSPARENT;
    }
    e.source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL;
    e.position=KS_KEYS+b->param2;
    e.layer=layer;
    struct encoder_tap tap = {.event = e, .code = code};
    for (int i = 0; i < (steps < 0 ? -steps : steps); i++) {
        if (k_msgq_put(&encoder_taps, &tap, K_NO_WAIT))
            break;
        pending_encoder_taps++;
    }
    k_mutex_unlock(&state_mutex);
    return ZMK_BEHAVIOR_OPAQUE;
}
static void encoder_run(void *a, void *b, void *c) {
    struct encoder_tap tap;
    for (;;) {
        k_msgq_get(&encoder_taps, &tap, K_FOREVER);
        k_mutex_lock(&state_mutex, K_FOREVER);
        tap.event.timestamp = k_uptime_get();
        execute(tap.code, tap.event, true);
        k_mutex_unlock(&state_mutex);
        k_msleep(5);
        k_mutex_lock(&state_mutex, K_FOREVER);
        tap.event.timestamp = k_uptime_get();
        execute(tap.code, tap.event, false);
        pending_encoder_taps--;
        k_mutex_unlock(&state_mutex);
    }
}
K_THREAD_DEFINE(keebart_encoder_thread, 4096, encoder_run, NULL, NULL, NULL, 8, 0, 0);

#endif
static const struct behavior_driver_api behavior_api = {
    .binding_pressed = pressed,
    .binding_released = released,
#if KS_ENCODERS
    .sensor_binding_accept_data = sensor_accept,
    .sensor_binding_process = sensor_process,
#endif
};
BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_api);

static uint8_t tap_count[KS_LAYERS];
static int64_t tap_press[KS_LAYERS], tap_release[KS_LAYERS];
static bool tap_latched[KS_LAYERS], lower_down, raise_down;
int ks_lighting_action(unsigned action, int value) {
    k_mutex_lock(&state_mutex, K_FOREVER);
    uint8_t *l = state.active.lighting;
    switch (action) {
    case 0:
        lighting_enabled = !lighting_enabled;
        break;
    case 1:
        lighting_enabled = value;
        break;
    case 2:
        state.active.mode = 0;
        l[0] = 4 + (l[0] + 41 + value - 4) % 41;
        break;
    case 3:
        l[3] += value;
        break;
    case 4:
        l[4] = CLAMP((int)l[4] + value, 0, 255);
        break;
    case 5:
        l[5] = CLAMP((int)l[5] + value, 0, 30);
        break;
    case 6:
        l[2] = CLAMP((int)l[2] + value, 0, 255);
        break;
    default:
        k_mutex_unlock(&state_mutex);
        return -EINVAL;
    }
    state.active.generation++;
    state.active.crc = ks_crc(&state.active, offsetof(struct ks_config, crc));
    state.staging = false;
    k_work_reschedule(&lighting_save_work, K_MSEC(1000));
    k_mutex_unlock(&state_mutex);
    return 0;
}
bool ks_lighting_on(void) { return lighting_enabled; }
static void macro_emit(void *ctx, uint16_t code, bool down) {
    struct zmk_behavior_binding_event e = {.position = KS_KEYS + KS_ENCODERS + KS_COMBOS + 512 + code, .timestamp = k_uptime_get(), .layer = 0};
    execute(code, e, down);
}
static void macro_run(struct k_work *work) {
    k_mutex_lock(&state_mutex, K_FOREVER);
    km_tick(&macro_vm, k_uptime_get(), macro_emit, NULL);
    if (macro_vm.active) k_work_reschedule(&macro_work, K_MSEC(5));
    k_mutex_unlock(&state_mutex);
}
static void dance_emit(void *ctx, unsigned position, uint16_t code, enum kd_action action) {
    struct zmk_behavior_binding_event e={.position=KS_KEYS+KS_ENCODERS+KS_COMBOS+256+position,.timestamp=k_uptime_get(),.layer=0,.source=ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL};
    execute(code,e,action!=KD_UP);
    if(action==KD_TAP){k_msleep(5);e.timestamp=k_uptime_get();execute(code,e,false);}
}
static void dance_tick(struct k_work *work) {
    k_mutex_lock(&state_mutex,K_FOREVER);
    kd_tick(&dances,k_uptime_get(),dance_emit,NULL);
    int64_t due=kd_deadline(&dances);
    if(due!=INT64_MAX)k_work_reschedule(&dance_work,K_MSEC(MAX(1,due-k_uptime_get())));
    k_mutex_unlock(&state_mutex);
}
static int execute(uint16_t c, struct zmk_behavior_binding_event e, bool down) {
    c=ksl_binding(state.active.locales,locale_runtime.active,c);
    if(locale_event(c,e,down))return 0;
    if(repeat_event(c,e,down))return 0;
    if(e.position<KS_KEYS+KS_ENCODERS+KS_COMBOS+256) {
        bool handled=kd_input(&dances,e.position,c,down,e.timestamp,state.active.dances,KD_COUNT,dance_emit,NULL);
        int64_t due=kd_deadline(&dances);
        if(due!=INT64_MAX)k_work_reschedule(&dance_work,K_MSEC(MAX(1,due-k_uptime_get())));
        if(handled)return 0;
    }

    if (c >= 0x7700 && c < 0x7700 + KM_COUNT) {
        if (down && km_start(&macro_vm, state.active.macros, KM_BYTES, c - 0x7700, k_uptime_get(), macro_emit, NULL))
            k_work_reschedule(&macro_work, K_NO_WAIT);
        return 0;
    }
    if (c == 41 && down && macro_vm.active && e.position < KS_KEYS + KS_ENCODERS + KS_COMBOS) { km_cancel(&macro_vm, macro_emit, NULL); return 0; }

    unsigned layer = c & 15;
    if (c >= 0x5000 && c <= 0x51ff) {
        layer = (c >> 5) & 15;
        uint8_t m = c & 15;
        if (c & 16)
            m <<= 4;
        struct zmk_behavior_binding mod = {.behavior_dev = NAME(kp), .param1 = mod_key(m)};
        if (down)
            zmk_keymap_layer_activate(layer);
        else if (!(locked_layers & BIT(layer)))
            zmk_keymap_layer_deactivate(layer);
        return zmk_behavior_invoke_binding(&mod, e, down);
    }
    if ((c & 0xfff0) == 0x5240 || (c & 0xfff0) == 0x52e0) {
        if (!down)
            return 0;
        k_mutex_lock(&state_mutex, K_FOREVER);
        if ((c & 0xfff0) == 0x52e0) {
            state.staged = state.active;
            state.staged.default_layer = layer;
            state.staged.generation++;
            state.staged.crc = ks_crc(&state.staged, offsetof(struct ks_config, crc));
            int err = persist(&state.staged);
            if (err) {
                k_mutex_unlock(&state_mutex);
                return err;
            }
            state.active = state.staged;
            state.staging = false;
        }
        uint8_t previous = base_layer;
        base_layer = layer;
        zmk_keymap_layer_deactivate(previous);
        zmk_keymap_layer_activate(layer);
        k_mutex_unlock(&state_mutex);
        return 0;
    }
    if ((c & 0xfff0) == 0x52c0) {
        if (down) {
            tap_press[layer] = e.timestamp;
            zmk_keymap_layer_activate(layer);
        } else {
            if (e.timestamp - tap_press[layer] < 200) {
                tap_count[layer] =
                    e.timestamp - tap_release[layer] < 200 ? tap_count[layer] + 1 : 1;
                if (tap_count[layer] >= 5) {
                    tap_latched[layer] = !tap_latched[layer];
                    tap_count[layer] = 0;
                }
                tap_release[layer] = e.timestamp;
            } else
                tap_count[layer] = 0;
            if (!tap_latched[layer] && !(locked_layers & BIT(layer)))
                zmk_keymap_layer_deactivate(layer);
        }
        return 0;
    }
    if (c == 0x7c7b) {
        if (down) {
            unsigned l = zmk_keymap_highest_layer_active();
            if (l != base_layer) {
                locked_layers ^= BIT(l);
                if (!(locked_layers & BIT(l)))
                    zmk_keymap_layer_deactivate(l);
            }
        }
        return 0;
    }
    if (c == 0x7c77 || c == 0x7c78) {
        if (c == 0x7c77)
            lower_down = down;
        else
            raise_down = down;
        unsigned l = c == 0x7c77 ? 1 : 2;
        if (down)
            zmk_keymap_layer_activate(l);
        else
            zmk_keymap_layer_deactivate(l);
        if (lower_down && raise_down)
            zmk_keymap_layer_activate(3);
        else
            zmk_keymap_layer_deactivate(3);
        return 0;
    }
    if ((c >= 0x7820 && c <= 0x782a) || (c >= 0x7e20 && c <= 0x7e22)) {
        if (!down)
            return 0;
        if (c == 0x7820 || c == 0x7e20)
            return ks_lighting_action(0, 0);
        if (c == 0x7e21 || c == 0x7e22)
            return ks_lighting_action(3, c == 0x7e21 ? 8 : -8);
        unsigned action = (c - 0x7821) / 2 + 2;
        int step = action == 5 ? 2 : action == 2 ? 1 : 8;
        return ks_lighting_action(action, (c & 1) ? step : -step);
    }
    if(c>=0x7c5d && c<=0x7c5f){ks_override_report(c,down);return 0;}
    struct zmk_behavior_binding b = translate(c);
    return zmk_behavior_invoke_binding(&b, e, down);
}
bool ks_frame(uint8_t out[KS_KEYS][3]) {
    k_mutex_lock(&state_mutex, K_FOREVER);
    bool awake = lighting_enabled && zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE;
    ks_render(&renderer, &state.active, zmk_keymap_highest_layer_active(), host_locks, awake,
              k_uptime_get(), out);
    bool streaming = awake && state.active.lighting[5] != 0;
    k_mutex_unlock(&state_mutex);
    return streaming;
}
static const uint8_t descriptor[] = {0x06, 0x60, 0xff, 0x09, 0x61, 0xa1, 0x01, 0x09, 0x62,
                                     0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x20,
                                     0x81, 0x02, 0x09, 0x63, 0x95, 0x20, 0x91, 0x02, 0xc0};
static int set_report(const struct device *dev, struct usb_setup_packet *setup, int32_t *len,
                      uint8_t **data) {
    if (setup->wValue != 0x0200 || *len != 32)
        return -EINVAL;
    return k_msgq_put(&requests, *data, K_NO_WAIT);
}
static void out_ready(const struct device *dev) {
    uint8_t data[64];
    uint32_t n = 0;
    if (hid_int_ep_read(dev, data, sizeof(data), &n) == 0 && n == 32)
        k_msgq_put(&requests, data, K_NO_WAIT);
}
static void in_ready(const struct device *dev) { k_sem_give(&tx_ready); }
static const struct hid_ops ops = {
    .set_report = set_report, .int_out_ready = out_ready, .int_in_ready = in_ready};
static int init(void) {
    ks_init(&state, ks_stock);
    ks_store_init(&storage);
    hid = device_get_binding("HID_1");
    if (!hid)
        return -ENODEV;
    usb_hid_register_device(hid, descriptor, sizeof(descriptor), &ops);
    return usb_hid_init(hid);
}
SYS_INIT(init, APPLICATION, 51);
static void run(void *a, void *b, void *c) {
    uint8_t input[32], output[32];
    for (;;) {
        k_msgq_get(&requests, input, K_FOREVER);
        k_mutex_lock(&state_mutex, K_FOREVER);
        state.storage_error=keebart_storage_error();
        uint8_t previous_default = state.active.default_layer;
        bool locale_changed = input[3]==10 || (input[3]==8 && memcmp(state.active.locales,state.staged.locales,KSL_BYTES));
        bool pending_keys = pending_encoder_taps != 0 || macro_vm.active || kd_busy(&dances);
        for (unsigned i = 0; i < KS_KEYS; i++)
            pending_keys |= combos.keys[i].state != 0;
        if ((input[3] == 10 ||
             (input[3] == 8 && ks_mapping_changed(&state.active, &state.staged))) &&
            pending_keys) {
            memset(output, 0, sizeof(output));
            memcpy(output, input, 5);
            output[5] = 5;
        } else {
            if (input[3]==15) ks_peer_release(state.peer_release);
            if(input[3]==10 || (input[3]==8 && ks_mapping_changed(&state.active,&state.staged)))ks_override_lifecycle_cancel();
            if(input[3]==17)locale_request(input,output);
            else if(input[3]==16)ksn_handle(input,output,!state.unlocking && !state.staging);
            else ks_handle(&state, input, sizeof(input), output, k_uptime_get(), persist, ks_stock);
        }
        if ((input[3] == 8 || input[3] == 10) && !output[5]) {
            if(locale_changed)locale_load();
            k_work_cancel_delayable(&lighting_save_work);
            if (previous_default != state.active.default_layer) {
                uint8_t previous = base_layer;
                base_layer = state.active.default_layer;
                zmk_keymap_layer_deactivate(previous);
                zmk_keymap_layer_activate(base_layer);
            }
        }
        if (input[3] == 10 && !output[5]) {
            locked_layers = 0;
            base_layer = 0;
            lighting_enabled = true;
            memset(tap_latched, 0, sizeof(tap_latched));
            memset(&combos, 0, sizeof(combos));
            memset(&renderer, 0, sizeof(renderer));
            zmk_keymap_layer_to(0);
        }
        k_mutex_unlock(&state_mutex);
        // Name persistence and advertising may wait for Bluetooth callbacks.
        // Never hold the key/render lock while doing that work. Only this USB
        // thread handles name transactions; the next request waits until done.
        if (input[3] == 10 && !output[5] && !ksn_reset()) output[5] = 6;
        ksn_task();
        if (k_sem_take(&tx_ready, K_MSEC(500)) == 0) {
            if (hid_int_ep_write(hid, output, 32, NULL) != 0)
                k_sem_give(&tx_ready);
        }
    }
}
K_THREAD_DEFINE(keebart_thread, 4096, run, NULL, NULL, NULL, 7, 0, 0);
