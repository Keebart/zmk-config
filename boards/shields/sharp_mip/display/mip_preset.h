/* Keebart Studio Sharp MiP display preset ABI (68x160 portrait, monochrome).
 * Shared between ZMK Studio firmware, host regression tests, and community
 * preset previews. Self-contained C11 with no OS or Zephyr headers.
 */
#ifndef KEEBART_STUDIO_MIP_PRESET_H
#define KEEBART_STUDIO_MIP_PRESET_H

#include <stdint.h>

#define KSM_WIDTH 68u
#define KSM_HEIGHT 160u
#define KSM_FB_BYTES ((KSM_WIDTH * KSM_HEIGHT) / 8u) /* 1360 bytes */
#define KSM_PROFILE_COUNT 5u
#define KSM_WPM_SAMPLES 10u

#define KSM_TRANSPORT_USB 0u
#define KSM_TRANSPORT_BLE 1u

#define KSM_MOD_CTRL (1u << 0)
#define KSM_MOD_SHIFT (1u << 1)
#define KSM_MOD_ALT (1u << 2)
#define KSM_MOD_GUI (1u << 3)

#define KSM_LOCK_NUM (1u << 0)
#define KSM_LOCK_CAPS (1u << 1)
#define KSM_LOCK_SCROLL (1u << 2)
#define KSM_LOCK_CAPS_WORD (1u << 3)

#define KSM_BATTERY_UNKNOWN 255u

typedef struct {
    /* Power & wireless link telemetry */
    uint8_t central_battery;      /* 0..100 or KSM_BATTERY_UNKNOWN */
    uint8_t central_charging;     /* 1 when central USB power present */
    uint8_t peripheral_battery;   /* 0..100 or KSM_BATTERY_UNKNOWN */
    uint8_t peripheral_charging;  /* 1 when peripheral USB power present */
    uint8_t peripheral_connected; /* 1 when split wireless link is active */

    /* Output transport & Bluetooth profiles (0..4) */
    uint8_t transport;          /* KSM_TRANSPORT_USB or KSM_TRANSPORT_BLE */
    uint8_t active_profile;     /* 0..4 */
    uint8_t profiles_bonded;    /* bitmask bit i = profile i bonded */
    uint8_t profiles_connected; /* bitmask bit i = profile i connected */

    /* Typing & keyboard state */
    uint8_t layer;                        /* 0..7 active layer index */
    uint8_t mods;                         /* KSM_MOD_* bitmask */
    uint8_t locks;                        /* KSM_LOCK_* bitmask */
    uint8_t wpm;                          /* current WPM */
    uint8_t wpm_history[KSM_WPM_SAMPLES]; /* oldest [0] .. newest [9] */
    uint32_t now_ms;                      /* event timestamp (avoid polling loops on battery!) */
    char layer_name[33];                  /* NUL-terminated custom Studio layer name */
    char last_key[24];                    /* NUL-terminated last key label */
} ksm_state_t;

/* Every Sharp MiP preset implements these two renderers.
 * Pixel bit = 1 draws foreground ink (dark on reflective silver, or bright if inverted).
 */
void ksm_preset_render_left(uint8_t fb[KSM_FB_BYTES], const ksm_state_t *s);
void ksm_preset_render_right(uint8_t fb[KSM_FB_BYTES], const ksm_state_t *s);

#endif /* KEEBART_STUDIO_MIP_PRESET_H */
