#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#define KD_COUNT 32
#define KD_ACTIVE 128
#define KD_BASE 0x5700
#define KD_GUARD 0x8000
#define KD_TERM_MASK 0x03ff
struct kd_entry { uint16_t tap, hold, twice, tap_hold, term; };
enum kd_action { KD_DOWN, KD_UP, KD_TAP };
typedef void (*kd_emit)(void *, unsigned, uint16_t, enum kd_action);
struct kd_state {
    struct kd_entry entry;
    int64_t due;
    uint16_t position, output;
    uint8_t count;
    bool used, down, resolved;
};
struct kd_engine { struct kd_state states[KD_ACTIVE]; };
bool kd_input(struct kd_engine *, unsigned, uint16_t, bool, int64_t, const struct kd_entry *, unsigned, kd_emit, void *);
void kd_tick(struct kd_engine *, int64_t, kd_emit, void *);
void kd_cancel(struct kd_engine *, kd_emit, void *);
bool kd_busy(const struct kd_engine *);
int64_t kd_deadline(const struct kd_engine *);
