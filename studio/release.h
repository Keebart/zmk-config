#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define KEEBART_RELEASE_ID 2026103105u
#define KEEBART_RELEASE_BYTES 12
static inline void keebart_release(uint8_t out[12], uint16_t model, uint8_t role, uint8_t storage) {
    memcpy(out,"KSV1",4);
    uint32_t release=KEEBART_RELEASE_ID;
    for (unsigned i=0;i<4;i++) out[4+i]=(uint8_t)(release>>(8*i));
    out[8]=(uint8_t)model;out[9]=(uint8_t)(model>>8);out[10]=role;out[11]=storage;
}
static inline bool keebart_release_matches(const uint8_t *data, unsigned length, uint16_t model, uint8_t role) {
    return length==12 && !memcmp(data,"KSV1",4) && data[8]==(uint8_t)model &&
        data[9]==(uint8_t)(model>>8) && data[10]==role && data[11]!=0;
}
