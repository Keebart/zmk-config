#pragma once
#include "locale.h"
#include "extended_runtime.h"
#define KSL_HELD 32
#define KSL_DEFERRED 64
struct ksl_io {
    void (*unicode)(void *,uint32_t);
    void (*tap)(void *,uint8_t,uint8_t);
    void (*sequence)(void *,uint8_t,uint8_t,bool);
    void (*action)(void *,uint16_t,bool);
    void *context;
    uint8_t (*mods)(void *);
    uint8_t (*locks)(void *);
    void (*prepare)(void *,uint16_t,bool);
    void (*pause)(void *);
    void (*wait)(void *,uint16_t); // generated tap duration, not a physical hold
    unsigned (*hand)(void *,unsigned); // 1 left, 2 right, 0 synthetic/exempt
    // Returns true when the typing engine owns the press (no synthetic repeat).
    bool (*character)(void *,uint16_t,bool,bool); // code, down, resolved tap/hold
    void (*cancel_typing)(void *);
    void (*replay)(void *,unsigned,uint16_t,bool,int64_t);
    uint16_t (*lookup)(void *,unsigned,uint16_t); // buffered physical press, current layers
    void (*stroke)(void *,uint8_t,bool); // held HID key; 0 refreshes modifier state

};
struct ksl_held {
    unsigned position;
    uint16_t code, hold, tap, output;
    uint8_t flags;
    int64_t due, started;
    bool used, resolved, forwarded, tap_active, interrupted;
};
struct ksl_deferred {
    unsigned position;
    uint16_t code;
    int64_t time;
    uint8_t mods;
    bool down, caps, tap;
};
struct ksl_character_hold { uint16_t code; uint8_t key, mods, suppress, count; };
struct ksl_runtime {
    struct ksl_character_hold characters[KSL_HELD];
    struct kse_runtime extended;
    struct ksl_deferred deferred[KSL_DEFERRED];
    unsigned deferred_count;
    bool draining;
    int64_t last_press_time, last_tap_time;
    unsigned last_tap_position;
    uint16_t last_press_code, last_tap_code;
    bool last_press_valid, last_tap_valid;
    struct ksl_held held[KSL_HELD];
    uint8_t active, requested;
    uint16_t layer_inputs[KSL_LAYER_RULES], layer_state;
    uint8_t layer_targets[KSL_LAYER_RULES];
    bool switching, guarded, sentence_default;
};
void ksl_runtime_init(struct ksl_runtime *,const uint8_t *);
void ksl_runtime_reload(struct ksl_runtime *,const uint8_t *);
bool ksl_managed_layer_tap(const struct ksl_runtime *,uint16_t);
bool ksl_event(struct ksl_runtime *,const uint8_t *,unsigned,uint16_t,bool,int64_t,uint8_t,bool,const struct ksl_io *);
void ksl_tick(struct ksl_runtime *,const uint8_t *,int64_t,uint8_t,bool,const struct ksl_io *);
void ksl_cancel(struct ksl_runtime *,const struct ksl_io *);
bool ksl_select(struct ksl_runtime *,const uint8_t *,unsigned);
int64_t ksl_deadline(const struct ksl_runtime *);

/* Volatile host selection, guarded atomically by endpoint and configuration CRC. */
bool ksl_usb_select(struct ksl_runtime *,const uint8_t *,const uint8_t *,uint8_t *,bool,bool);

void ksl_emit_character(const uint8_t *,unsigned,uint16_t,uint8_t,bool,const struct ksl_io *);

bool ksl_extended_event(struct ksl_runtime *,const uint8_t *,unsigned,uint16_t,bool,int64_t,uint8_t,bool,const struct ksl_io *);

bool ksl_text(struct ksl_runtime *,const uint8_t *,uint32_t,int64_t,const struct ksl_io *);
void ksl_emit_unicode(struct ksl_runtime *,const uint8_t *,uint32_t,int64_t,const struct ksl_io *);

// Single-stroke characters retain their HID press until physical release.
bool ksl_character_press(struct ksl_runtime *,const uint8_t *,uint16_t,uint16_t,uint8_t,bool,const struct ksl_io *);
void ksl_character_release(struct ksl_runtime *,uint16_t,const struct ksl_io *);
uint8_t ksl_character_report_mods(const struct ksl_runtime *,uint8_t);
