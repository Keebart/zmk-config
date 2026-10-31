#include "locales/alt_repeat.h"
static struct kar_state repeat_state;
extern void ks_repeat_weak(uint8_t);
static void repeat_emit(void *ctx,unsigned pos,uint16_t code,uint8_t mods,bool down){
    (void)ctx;(void)mods;
    if(down)ks_repeat_weak(kar_mods(&repeat_state));
    struct zmk_behavior_binding_event e={.position=KS_KEYS+KS_ENCODERS+KS_COMBOS+2048+pos,.timestamp=k_uptime_get(),.layer=zmk_keymap_highest_layer_active(),.source=ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL};
    execute(code,e,down);
    if(!down)ks_repeat_weak(kar_mods(&repeat_state));
}
void ks_repeat_note(uint16_t code,uint8_t mods){if(!macro_vm.active)kar_note(&repeat_state,code,mods);}
static bool repeat_event(uint16_t code,struct zmk_behavior_binding_event e,bool down){
    if(kar_event(&repeat_state,state.active.repeats,e.position,code,down,repeat_emit,NULL))return true;
    if(down&&!(code>=0x2000&&code<0x5000))ks_repeat_note(code,ks_locale_mods());
    return false;
}
static void repeat_cancel(void){kar_cancel(&repeat_state,repeat_emit,NULL);}
