#include "override_bridge.h"
#include "override_engine/process_key_override.h"
#include "override_engine/quantum.h"
#include <string.h>
#include "locales/locale.h"
static const struct ks_override *entries;
static const struct ks_override_io *io;
static key_override_t rules[KS_OVERRIDE_COUNT];
static bool disabled;
extern void ks_override_engine_reset(void);
extern bool ks_override_engine_pending(void);
void ks_override_init(const struct ks_override *data,const struct ks_override_io *callbacks){entries=data;io=callbacks;ks_override_engine_reset();}
uint16_t key_override_count(void){return KS_OVERRIDE_COUNT;}
const key_override_t *key_override_get(uint16_t n){
    if(n>=KS_OVERRIDE_COUNT)return NULL;
    const struct ks_override *e=&entries[n];key_override_t *r=&rules[n];
    *r=(key_override_t){.trigger=e->trigger,.replacement=e->replacement,.layers=e->layers,.trigger_mods=e->trigger_mods,.negative_mod_mask=e->negative_mod_mask,.suppressed_mods=e->suppressed_mods,.options=e->options&63,.enabled=(e->options&128)?NULL:&disabled};
    return r;
}
uint8_t extract_mod_bits(uint16_t c){if(c<0x100||c>0x1fff)return 0;uint8_t m=(c>>8)&15;return (c&0x1000)?m<<4:m;}
uint8_t get_mods(void){return io->mods();}
void set_weak_override_mods(uint8_t m){io->weak(m);}
void clear_weak_override_mods(void){io->weak(0);}
void set_suppressed_override_mods(uint8_t m){io->mask(m);}
void clear_suppressed_override_mods(void){io->mask(0);}
uint32_t timer_read32(void){return io->clock();}
uint32_t timer_elapsed32(uint32_t at){return timer_read32()-at;}
uint8_t read_source_layers_cache(keypos_t p){return p.row;}
void wait_ms(unsigned n){io->wait(n);}
void send_keyboard_report(void){io->send();}
void add_key(uint8_t c){io->press(c);}
void del_key(uint8_t c){io->release(c);}
void register_code(uint8_t c){io->press(c);io->send();}
void unregister_code(uint8_t c){io->release(c);io->send();}
void register_code16(uint16_t c){if(ksl_character(c)){if(io->character)io->character(c,true);return;}uint8_t m=extract_mod_bits(c);if(m)io->shortcut(m);register_code(c&255);}
bool ks_override_process(uint16_t c,uint8_t layer,bool down){keyrecord_t r={.event={.key={.row=layer},.pressed=down}};return process_key_override(c,&r);}
void ks_override_tick(void){key_override_task();}
bool ks_override_pending(void){return ks_override_engine_pending();}
void ks_override_cancel(void){if(io)ks_override_engine_reset();}

void ksl_override_release(uint16_t c){if(ksl_character(c)){if(io->character)io->character(c,false);}else unregister_code(c);}
