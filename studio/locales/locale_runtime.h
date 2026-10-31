#pragma once
#include "locale.h"
#define KSL_HELD 32
#define KSL_DEFERRED 64
struct ksl_io {
    void (*tap)(void *,uint8_t,uint8_t);
    void (*action)(void *,uint16_t,bool);
    void *context;
    uint8_t (*mods)(void *);
    void (*prepare)(void *,uint16_t,bool);
    void (*pause)(void *);
    unsigned (*hand)(void *,unsigned); // 1 left, 2 right, 0 synthetic/exempt
    void (*replay)(void *,unsigned,uint16_t,bool,int64_t);
};
struct ksl_held {
    unsigned position;
    uint16_t code, hold, tap;
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
struct ksl_runtime {
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
    bool switching, guarded;
};
void ksl_runtime_init(struct ksl_runtime *,const uint8_t *);
bool ksl_event(struct ksl_runtime *,const uint8_t *,unsigned,uint16_t,bool,int64_t,uint8_t,bool,const struct ksl_io *);
void ksl_tick(struct ksl_runtime *,const uint8_t *,int64_t,uint8_t,bool,const struct ksl_io *);
void ksl_cancel(struct ksl_runtime *,const struct ksl_io *);
bool ksl_select(struct ksl_runtime *,const uint8_t *,unsigned);
int64_t ksl_deadline(const struct ksl_runtime *);

/* Volatile host selection, guarded atomically by endpoint and configuration CRC. */
bool ksl_usb_select(struct ksl_runtime *,const uint8_t *,const uint8_t *,uint8_t *,bool,bool);
