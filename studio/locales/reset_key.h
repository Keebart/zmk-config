// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define KS_RESET_KEY 0x7c03
// Only physical position events may arm this guard. Generated actions (macros,
// combos, encoders, tap dances) must never be passed here.
struct ks_reset_key { uint32_t since, released; unsigned position; bool held, pending; };
static inline void ks_reset_key_cancel(struct ks_reset_key *s) { s->held=false; s->pending=false; }
static inline void ks_reset_key_event(struct ks_reset_key *s, unsigned pos, uint16_t code,
                                      bool down, uint32_t now, bool blocked) {
    if(blocked){ks_reset_key_cancel(s);return;}
    if(down) {
        if(s->held && s->position==pos)return;
        if(s->held || s->pending){ks_reset_key_cancel(s);return;}
        if(code==KS_RESET_KEY){s->held=true;s->position=pos;s->since=now;}
    } else if(s->held && s->position==pos) {
        s->held=false;s->pending=(uint32_t)(now-s->since)>=2500;s->released=now;
    }
}
static inline bool ks_reset_key_take(struct ks_reset_key *s, uint32_t now, bool all_released, bool blocked) {
    if(blocked || (s->pending && (uint32_t)(now-s->released)>3000))ks_reset_key_cancel(s);
    if(!s->pending || !all_released)return false;
    s->pending=false;return true;
}
