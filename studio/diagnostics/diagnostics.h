#pragma once
/* Bounded operational flight recorder. No keys, addresses, names or flash writes.
 * Caller serializes access. Retained RAM is best-effort across warm reset only. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define KDG_CAPACITY 32
#define KDG_BOOT 1
#define KDG_FATAL 2
#define KDG_USB 3
#define KDG_BT_CONNECTED 4
#define KDG_BT_DISCONNECTED 5
#define KDG_BT_SECURITY 6
#define KDG_SPLIT 7
#define KDG_STORAGE 8
#define KDG_PC 9
#define KDG_BOOT_INDEX (UINT32_MAX-1)
#define KDG_FATAL_INDEX UINT32_MAX
struct kdg_entry { uint32_t seq, boot, ms, kind, value, aux, crc; };
struct kdg_store {
    uint32_t magic, release;
    struct kdg_entry ring[KDG_CAPACITY], boot_record, fatal;
};
struct kdg_runtime { struct kdg_store *store; uint32_t latest, boot, release; uint8_t role; bool retained; };
static inline uint32_t kdg_crc(const struct kdg_entry *e) {
    const uint8_t *p=(const uint8_t *)e; uint32_t c=0xffffffffu;
    for(unsigned i=0;i<24;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^(0xedb88320u & (0u-(c&1u)));}
    return ~c;
}
static inline bool kdg_valid(const struct kdg_entry *e) {
    return e->seq && e->kind>=KDG_BOOT && e->kind<=KDG_PC && e->crc==kdg_crc(e);
}
static inline void kdg_add(struct kdg_runtime *r,uint32_t ms,uint32_t kind,uint32_t value,uint32_t aux) {
    if(!r->store)return;
    if(r->latest>=UINT32_MAX-2){memset(r->store->ring,0,sizeof(r->store->ring));r->latest=0;}
    struct kdg_entry e={.seq=++r->latest,.boot=r->boot,.ms=ms,.kind=kind,.value=value,.aux=aux};e.crc=kdg_crc(&e);
    struct kdg_entry *slot=&r->store->ring[(e.seq-1)%KDG_CAPACITY];
    slot->crc=0; *slot=e;
    if(kind==KDG_BOOT)r->store->boot_record=e;
    if(kind==KDG_FATAL)r->store->fatal=e;
}
static inline void kdg_init(struct kdg_runtime *r,struct kdg_store *s,uint32_t release,uint8_t role,uint32_t reset) {
    memset(r,0,sizeof(*r));r->store=s;r->release=release;r->role=role;
    if(s->magic==0x3147444b && s->release==release){
        for(unsigned i=0;i<KDG_CAPACITY;i++)if(kdg_valid(&s->ring[i]) && s->ring[i].seq>r->latest){r->latest=s->ring[i].seq;r->boot=s->ring[i].boot;}
        r->retained=r->latest!=0;
    }else{memset(s,0,sizeof(*s));s->magic=0x3147444b;s->release=release;}
    r->boot++;kdg_add(r,0,KDG_BOOT,reset,r->retained);
}
static inline void kdg_u32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static inline uint32_t kdg_get(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline void kdg_encode(uint8_t out[24],const struct kdg_entry *e){
    kdg_u32(out,e->seq);kdg_u32(out+4,e->boot);kdg_u32(out+8,e->ms);kdg_u32(out+12,e->kind);kdg_u32(out+16,e->value);kdg_u32(out+20,e->aux);
}
static inline bool kdg_read(struct kdg_runtime *r,uint32_t index,uint32_t now,uint8_t out[24]) {
    memset(out,0,24);if(!r->store)return false;
    if(!index){memcpy(out,"KDG1",4);kdg_u32(out+4,r->release);kdg_u32(out+8,r->boot);kdg_u32(out+12,r->latest);out[16]=r->role;out[17]=KDG_CAPACITY;out[18]=r->retained;out[19]=r->latest<KDG_CAPACITY?r->latest:KDG_CAPACITY;kdg_u32(out+20,now);return true;}
    const struct kdg_entry *e=index==KDG_FATAL_INDEX?&r->store->fatal:index==KDG_BOOT_INDEX?&r->store->boot_record:&r->store->ring[(index-1)%KDG_CAPACITY];
    if(!kdg_valid(e) || (index<KDG_BOOT_INDEX && e->seq!=index))return false;
    kdg_encode(out,e);return true;
}
