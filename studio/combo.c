/* Native QMK combo processing, namespaced for Studio BT. Original physical
 * keycodes/timestamps remain available for Tap Dance fallback. */
#include "combo.h"
#include "combo_trigger.h"
#include "combo_engine/process_combo.h"
#define KC_COOKIE 0x4b434f4du
static struct ks_combo_engine *current;
static const struct ks_config *config;
static ks_emit output;
static void *context;
static int64_t clock_ms;
combo_t key_combos[64];
static uint16_t trigger_keys[64][5];
extern void kc_engine_reset(void);
extern int64_t kc_engine_deadline(int64_t);
uint16_t kc_clock(void){return (uint16_t)clock_ms;}
uint16_t kc_elapsed(uint16_t at){return (uint16_t)(kc_clock()-at);}
uint16_t kc_term(void){return config->keys[KS_TERM_AT];}
uint16_t kc_trigger_code(const uint16_t *keys,uint16_t code){
    uint16_t tap=code>=KD_BASE && code<KD_BASE+KD_COUNT?config->dances[code-KD_BASE].tap:0;
    return ks_combo_trigger_code(keys,code,tap);
}
uint16_t keymap_key_to_keycode(uint8_t l,keypos_t p){return l<KS_LAYERS&&p.col<KS_KEYS?config->keys[l*KS_KEYS+p.col]:0;}
void kc_record(keyrecord_t *record){
    bool combo=record->event.type==COMBO_EVENT;
    unsigned position=combo?KS_KEYS+KS_ENCODERS+record->event.key.col:record->event.key.col;
    uint16_t age=(uint16_t)(kc_clock()-record->event.time);
    int64_t at=combo||age>=0x8000?clock_ms:clock_ms-age;
    output(context,position,record->keycode,record->event.pressed,at);
}
static void bind(struct ks_combo_engine *s,const struct ks_config *c,int64_t at,ks_emit emit,void *arg){
    if(current!=s || s->cookie!=KC_COOKIE){
        kc_engine_reset();memset(key_combos,0,sizeof(key_combos));s->cookie=KC_COOKIE;current=s;
        for(unsigned i=0;i<64;i++)key_combos[i].keys=trigger_keys[i];
    }
    config=c;clock_ms=at;output=emit;context=arg;
    for(unsigned i=0;i<64;i++){
        uint16_t extra[5];const uint16_t *entry;
        if(i<KS_COMBOS)entry=c->keys+KS_COMBO_AT+i*5;
        else {ksl_extra_combo(c->locales,i-KS_COMBOS,extra);entry=extra;}
        memcpy(trigger_keys[i],entry,4*sizeof(uint16_t));
        trigger_keys[i][4]=0; // Native QMK requires a terminator after four triggers.
        key_combos[i].keycode=entry[4];
    }
}
int64_t ks_combo_deadline(const struct ks_combo_engine *s,const struct ks_config *c){
    (void)c;return s==current&&s->cookie==KC_COOKIE?kc_engine_deadline(clock_ms):INT64_MAX;
}
void ks_combo_tick(struct ks_combo_engine *s,const struct ks_config *c,int64_t at,ks_emit emit,void *arg){
    bind(s,c,at,emit,arg);combo_task();
}
void ks_combo_input(struct ks_combo_engine *s,const struct ks_config *c,unsigned p,uint16_t code,bool down,int64_t at,ks_emit emit,void *arg){
    if(p>=KS_KEYS)return;
    bind(s,c,at,emit,arg);combo_task();
    if(down){if(s->keys[p].state)return;s->keys[p]=(struct ks_combo_key){code,1};}
    else {if(!s->keys[p].state)return;code=s->keys[p].code;}
    keyrecord_t record={.event={.key={0,p},.type=KEY_EVENT,.time=(uint16_t)at,.pressed=down},.keycode=code};
    if(process_combo(code,&record))kc_record(&record);
    if(!down)s->keys[p]=(struct ks_combo_key){0};
}
