#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "hardware.h"
#include "macro_vm.h"
#include "locales/locale.h"
#include "locales/alt_repeat.h"
#include "tap_dance.h"
#include "override_bridge.h"
#define KS_LAYERS 16
#define KS_KEY_COUNT (KS_KEYS * KS_LAYERS)
#define KS_ENCODER_AT KS_KEY_COUNT
#define KS_COMBO_AT (KS_ENCODER_AT + KS_LAYERS * KS_ENCODERS * 2)
#define KS_COMBOS 32
#define KS_TERM_AT (KS_COMBO_AT + KS_COMBOS * 5)
#define KS_COUNT (KS_TERM_AT + 1)
#define KS_PALETTE_SIZE (KS_LAYERS * KS_KEYS * 7)
#define KS_DATA_SIZE (KS_COUNT * 2 + 6 + KS_PALETTE_SIZE + 4 + KM_BYTES + KD_COUNT * 10 + KS_OVERRIDE_COUNT * 10 + KSL_BYTES + KAR_COUNT * 6)
#define KS_CAPABILITIES 1023
struct ks_config {
    uint32_t version;
    uint32_t generation;
    uint16_t keys[KS_COUNT];
    uint8_t lighting[6];                      /* effect LE16, speed, hue, saturation, value */
    uint8_t palette[KS_PALETTE_SIZE];         /* all RGB layers, then all indicator layers */
    uint8_t mode, default_layer, reserved[2]; /* 0 animation, 1 per-key */
    uint8_t macros[KM_BYTES];
    struct kd_entry dances[KD_COUNT];
    struct ks_override overrides[KS_OVERRIDE_COUNT];
    uint8_t locales[KSL_BYTES];
    struct kar_rule repeats[KAR_COUNT];
    uint32_t crc;
};
struct ks_state {
    uint8_t peer_release[12];
    int32_t storage_error;
    struct ks_config active, staged;
    uint8_t received[(KS_DATA_SIZE + 7) / 8];
    uint16_t rotations[KS_ENCODER_SLOTS][2];
    int64_t rotation_remainder[KS_ENCODER_SLOTS];
    int rotation_steps[KS_ENCODER_SLOTS];
    uint64_t pressed;
    uint32_t transaction;
    int64_t hold_since, unlock_since, last_request;
    bool unlocked, unlocking, staging;
};
typedef int (*ks_persist)(const struct ks_config *);
uint32_t ks_crc(const void *, size_t);
bool ks_supported(uint16_t);
void ks_init(struct ks_state *, const uint16_t *stock);
bool ks_restore(struct ks_state *, const struct ks_config *);
void ks_lock(struct ks_state *);
void ks_position(struct ks_state *, unsigned, bool, int64_t);
void ks_handle(struct ks_state *, const uint8_t *, size_t, uint8_t[32], int64_t, ks_persist,
               const uint16_t *);

uint16_t ks_binding(const struct ks_state *, unsigned layer, unsigned position);
void ks_rotate(struct ks_state *, unsigned encoder, int32_t degrees, int32_t micro,
               unsigned triggers);
bool ks_config_valid(const struct ks_config *);

bool ks_mapping_changed(const struct ks_config *, const struct ks_config *);
