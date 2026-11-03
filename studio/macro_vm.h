#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define KM_BYTES 4096
#define KM_COUNT 32
#define KM_HELD 16
struct km_vm {
    const uint8_t *bytes;
    size_t at, end;
    uint16_t held[KM_HELD], tap;
    uint8_t held_count;
    int64_t due;
    bool active;
};
typedef void (*km_emit)(void *, uint16_t, bool);
bool km_valid(const uint8_t *, size_t, unsigned);
void km_cancel(struct km_vm *, km_emit, void *);
bool km_start(struct km_vm *, const uint8_t *, size_t, unsigned, int64_t, km_emit, void *);
void km_tick(struct km_vm *, int64_t, km_emit, void *);
