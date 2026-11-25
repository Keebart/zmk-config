#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Initial cold-boot scan window (ms after APPLICATION init). kscan debounces
 * an already-held key in ~5-15 ms; 250 ms covers boot jitter while staying
 * well below manual plug-then-type timing. */
#define KS_BOOTMAGIC_COLD_MS 250u

struct ks_bootmagic {
    uint64_t held;
    uint32_t boot_ms;
    uint8_t magic;
    bool ready;
    bool vbus;
    bool host;
    bool armed;
};

static inline void ks_bootmagic_init(struct ks_bootmagic *s, uint8_t magic) {
    s->magic = magic;
}

static inline bool ks_bootmagic_only_magic(const struct ks_bootmagic *s) {
    return s->magic < 64u && s->held == (1ull << s->magic);
}

static inline bool ks_bootmagic_triggered(const struct ks_bootmagic *s) {
    return s->armed && s->vbus && s->host && ks_bootmagic_only_magic(s);
}

static inline bool ks_bootmagic_boot_ready(struct ks_bootmagic *s, uint32_t now_ms, bool vbus,
                                           bool host) {
    s->boot_ms = now_ms;
    s->ready = true;
    if (!vbus) {
        s->vbus = false;
        s->host = false;
        s->armed = false;
        return false;
    }
    s->vbus = true;
    if (host) {
        s->host = true;
    }
    if (ks_bootmagic_only_magic(s)) {
        s->armed = true;
    }
    return ks_bootmagic_triggered(s);
}

static inline bool ks_bootmagic_key(struct ks_bootmagic *s, uint32_t position, bool pressed,
                                    uint32_t now_ms) {
    if (position < 64u) {
        uint64_t bit = 1ull << position;
        if (pressed) {
            s->held |= bit;
        } else {
            s->held &= ~bit;
        }
    }
    if (!ks_bootmagic_only_magic(s)) {
        s->armed = false;
        return false;
    }
    /* Cold boot: if USB was already attached before kscan finished its initial
     * debounce scan, arm within the boot window. */
    if (pressed && s->vbus &&
        (!s->ready || (uint32_t)(now_ms - s->boot_ms) <= KS_BOOTMAGIC_COLD_MS)) {
        s->armed = true;
    }
    return ks_bootmagic_triggered(s);
}

static inline bool ks_bootmagic_usb(struct ks_bootmagic *s, bool vbus, bool host) {
    bool prev_vbus = s->vbus;
    if (!vbus) {
        s->vbus = false;
        s->host = false;
        s->armed = false;
        return false;
    }
    s->vbus = true;
    if (host) {
        s->host = true;
    }
    /* Live USB plug-in (battery ON -> cable attached): arm only when the magic
     * key is already the sole held key as VBUS appears. */
    if (!prev_vbus && ks_bootmagic_only_magic(s)) {
        s->armed = true;
    }
    return ks_bootmagic_triggered(s);
}
