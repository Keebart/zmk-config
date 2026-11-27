#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define KSLS_VERSION 1u
#define KSLS_PAYLOAD_BYTES 19u
#define KSLS_MAX_MATRIX_BYTES 10u

#define KSLS_FLAG_CAPS_LOCK        (1u << 0)
#define KSLS_FLAG_NUM_LOCK         (1u << 1)
#define KSLS_FLAG_SCROLL_LOCK      (1u << 2)
#define KSLS_FLAG_CAPS_WORD        (1u << 3)
#define KSLS_FLAG_MATRIX_UNLOCKED  (1u << 4)
#define KSLS_FLAG_MATRIX_POSITIONS (1u << 5)

struct ksls_snapshot {
    uint8_t active_layer;
    uint8_t default_layer;
    uint16_t layer_mask;
    uint8_t modifiers;
    uint8_t flags;
    uint8_t computer;
    uint8_t matrix_len;
    uint8_t matrix[KSLS_MAX_MATRIX_BYTES];
};

/* Pack volatile live overlay telemetry. Matrix bytes are strictly zeroed unless unlocked. */
static inline void ksls_pack(uint8_t out[KSLS_PAYLOAD_BYTES], const struct ksls_snapshot *s,
                             bool unlocked, bool positions_format) {
    memset(out, 0, KSLS_PAYLOAD_BYTES);
    uint8_t flags = s->flags & (KSLS_FLAG_CAPS_LOCK | KSLS_FLAG_NUM_LOCK |
                                KSLS_FLAG_SCROLL_LOCK | KSLS_FLAG_CAPS_WORD);
    if (positions_format) {
        flags |= KSLS_FLAG_MATRIX_POSITIONS;
    }
    uint8_t def = s->default_layer & 15u;
    uint16_t mask = s->layer_mask | (uint16_t)(1u << def);
    out[0] = KSLS_VERSION;
    out[1] = s->active_layer;
    out[2] = def;
    out[3] = (uint8_t)(mask & 0xffu);
    out[4] = (uint8_t)(mask >> 8);
    out[5] = s->modifiers;
    out[7] = s->computer;
    if (unlocked) {
        uint8_t n = s->matrix_len;
        if (n > KSLS_MAX_MATRIX_BYTES) {
            n = KSLS_MAX_MATRIX_BYTES;
        }
        flags |= KSLS_FLAG_MATRIX_UNLOCKED;
        out[8] = n;
        if (n) {
            memcpy(out + 9, s->matrix, n);
        }
    }
    out[6] = flags;
}
