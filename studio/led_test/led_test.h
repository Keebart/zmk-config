#pragma once
#include <stdbool.h>
#include <stdint.h>
#define KSLT_LEASE_MS 2000u
struct kslt_state { uint32_t refreshed; uint8_t channel; };
/* Fixed RGB channels only. RAM-only lease, unsigned subtraction handles wrap. */
static inline bool kslt_set(struct kslt_state *s,uint8_t channel,uint32_t now,bool authorized) {
    if(channel>3 || (channel && !authorized))return false;
    s->channel=channel;s->refreshed=now;return true;
}
static inline uint8_t kslt_channel(struct kslt_state *s,uint32_t now) {
    if(now-s->refreshed>=KSLT_LEASE_MS)s->channel=0;
    return s->channel;
}
static inline void kslt_rgb(uint8_t channel,uint8_t limit,uint8_t out[3]) {
    for(unsigned i=0;i<3;i++)out[i]=channel==i+1?limit:0;
}
