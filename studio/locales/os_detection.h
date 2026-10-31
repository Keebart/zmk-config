#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* Passive USB string-request fingerprints; no descriptors or host reports change.
 * Unknown/ambiguous traffic is deliberately not assigned an OS. */
enum kso_platform { KSO_UNKNOWN, KSO_WINDOWS, KSO_MACOS, KSO_LINUX };
struct kso_capture {
    uint16_t lengths[64];
    uint8_t count, os;
    bool overflow, configured, done;
    uint32_t epoch, last_ms, ready_ms;
};
static inline void kso_reset(struct kso_capture *s){uint32_t epoch=s->epoch+1;memset(s,0,sizeof(*s));s->epoch=epoch;}
static inline void kso_configure(struct kso_capture *s,uint32_t now){if(!s->configured){s->configured=true;s->ready_ms=now;}}
static inline void kso_observe(struct kso_capture *s,uint16_t length,uint32_t now){
    if(s->done)return;
    s->last_ms=now;
    if(s->count<64)s->lengths[s->count++]=length;else s->overflow=true;
}
static inline uint8_t kso_classify(const struct kso_capture *s){
    if(s->overflow||s->count<3)return KSO_UNKNOWN;
    unsigned full=0,short_header=0;
    for(unsigned i=0;i<s->count;i++){full+=s->lengths[i]==255;short_header+=s->lengths[i]==4;}
    if(full==s->count)return KSO_LINUX;
    if(s->lengths[0]==2){
        unsigned i=0,pairs=0;
        while(i+1<s->count&&s->lengths[i]==2&&s->lengths[i+1]>=4&&s->lengths[i+1]<=254&&!(s->lengths[i+1]&1)){pairs++;i+=2;}
        if(pairs>=2&&i<s->count){while(i<s->count&&s->lengths[i]==255)i++;if(i==s->count)return KSO_MACOS;}
        return KSO_UNKNOWN;
    }
    if(full>=2&&short_header)return KSO_WINDOWS;
    return KSO_UNKNOWN;
}
static inline bool kso_ready(struct kso_capture *s,uint32_t now){
    if(!s->configured)return false;
    if(!s->done){
        if((uint32_t)(now-s->last_ms)<500||(uint32_t)(now-s->ready_ms)<500)return false;
        s->os=kso_classify(s);s->done=true;
    }
    return true;
}
