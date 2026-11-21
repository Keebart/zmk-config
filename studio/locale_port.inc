// Included by the Studio adapter after translate(), under its existing mutex.
#include "locales/locale_runtime.h"
#include <zmk/hid.h>
#include "rollover/rollover.h"
#include "rollover/zmk.h"
#include <zmk/endpoints.h>
#include <zmk/events/keycode_state_changed.h>
extern void ks_override_locale_tap(uint8_t,uint8_t);
static struct ksl_runtime locale_runtime;
static unsigned locale_bypass;
bool ks_locale_resolved_action(void){return locale_bypass!=0;}
static bool locale_replaying;
static void locale_replay(void *ctx,unsigned pos,uint16_t code,bool down,int64_t time){
    (void)ctx;struct zmk_behavior_binding_event e={.position=pos,.timestamp=time,.layer=0,.source=ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL};
    bool previous=locale_replaying;locale_replaying=true;execute(code,e,down);locale_replaying=previous;
}
static void locale_action(void *ctx,uint16_t code,bool down){
    if(code>=KSN_ON&&code<=KSN_TOGGLE){ks_nkro_action(code,down);return;}
    if(code==KSE_JIGGLER){pointer_key(code,down);return;}
    (void)ctx;
    struct zmk_behavior_binding_event e={.position=KS_KEYS+KS_ENCODERS+KS_COMBOS+1024+code,.timestamp=k_uptime_get(),.layer=0,.source=ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL};
    locale_bypass++;execute(code,e,down);locale_bypass--;
}
static void locale_pause(void *ctx,uint16_t ms){(void)ctx;if(ms)k_msleep(ms);}
extern void ks_override_sequence(uint8_t,uint8_t,bool);
static void locale_sequence(void *ctx,uint8_t key,uint8_t mods,bool end){(void)ctx;ks_override_sequence(key,mods,end);}
static void locale_tap(void *ctx,uint8_t key,uint8_t mods){(void)ctx;ks_override_locale_tap(key,mods);}
extern uint8_t ks_locale_mods(void);
extern bool ks_locale_override_process(uint16_t,uint8_t,bool);
static uint8_t locale_locks(void *ctx){(void)ctx;return host_locks;}
static uint8_t locale_mods(void *ctx){(void)ctx;return ks_locale_mods();}
static void locale_prepare(void *ctx,uint16_t code,bool down){
    (void)ctx;
    // A logical action lets sticky modifiers observe one character, including
    // a two-stroke composition. This vendor usage never reaches a host report.
    raise_zmk_keycode_state_changed((struct zmk_keycode_state_changed){.usage_page=0xff60,.keycode=code,.state=down,.timestamp=k_uptime_get()});
}
static unsigned locale_hand(void *ctx,unsigned pos){
    (void)ctx;if(pos>=KS_KEYS)return 0;
    extern const uint8_t ks_led_positions[KS_KEYS];
    for(unsigned i=0;i<KS_HALF_LEDS;i++)if(ks_led_positions[i]==pos)return 1;
    return 2;
}
extern bool ks_typing_character(uint16_t,bool,bool);
extern void ks_word_cancel(void);
static bool locale_character(void *ctx,uint16_t code,bool down,bool resolved){(void)ctx;return ks_typing_character(code,down,resolved);}
static void locale_cancel_typing(void *ctx){(void)ctx;ks_word_cancel();}
extern void ks_locale_stroke(uint8_t,bool);
static void locale_stroke(void *ctx,uint8_t key,bool down){(void)ctx;ks_locale_stroke(key,down);}
uint8_t ks_locale_report_mask(void){return ~ksl_character_report_mods(&locale_runtime,255);}
uint8_t ks_locale_report_mods(uint8_t mods){return ksl_character_report_mods(&locale_runtime,mods);}
static uint16_t locale_lookup(void *ctx,unsigned pos,uint16_t code){
    (void)ctx;if(pos>=KS_KEYS)return code;
    unsigned mapped=locale_runtime.extended.swap?kse_swap_position(state.active.locales,pos):pos;
    for(int layer=KS_LAYERS-1;layer>=0;layer--)if(zmk_keymap_layer_active(layer)){
        uint16_t binding=ksl_binding(state.active.locales,locale_runtime.active,ks_binding(&state,layer,mapped));
        if(binding!=1)return binding;
    }
    return 0;
}
static const struct ksl_io locale_io;
static void locale_schedule(void);
static bool locale_text(void *ctx,uint32_t c){(void)ctx;bool accepted=ksl_text(&locale_runtime,state.active.locales,c,k_uptime_get(),&locale_io);locale_schedule();return accepted;}
static void locale_unicode(void *ctx,uint32_t c){(void)ctx;ksl_emit_unicode(&locale_runtime,state.active.locales,c,k_uptime_get(),&locale_io);locale_schedule();}
static const struct ksl_io locale_io={.unicode=locale_unicode,.tap=locale_tap,.sequence=locale_sequence,.character=locale_character,.cancel_typing=locale_cancel_typing,.action=locale_action,.mods=locale_mods,.locks=locale_locks,.prepare=locale_prepare,.wait=locale_pause,.replay=locale_replay,.hand=locale_hand,.stroke=locale_stroke,.lookup=locale_lookup};
uint16_t ks_locale_tap_delay(uint16_t code){return ksl_tap_delay(state.active.locales,ksl_binding(state.active.locales,locale_runtime.active,code));}
unsigned ks_locale_computer(void){return locale_runtime.active;}
bool ks_locale_character_press(uint16_t source,uint16_t code,uint8_t mods){return ksl_character_press(&locale_runtime,state.active.locales,source,code,mods,(host_locks&2)!=0,&locale_io);}
void ks_locale_character_release(uint16_t code){ksl_character_release(&locale_runtime,code,&locale_io);}
void ks_locale_character_emit(uint16_t code,uint8_t mods){ksl_emit_character(state.active.locales,locale_runtime.active,code,mods,(host_locks&2)!=0,&locale_io);}
static void locale_work_run(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(locale_work,locale_work_run);
static void locale_schedule(void){int64_t due=ksl_deadline(&locale_runtime);if(locale_runtime.switching&&!state.pressed)due=MIN(due,k_uptime_get()+1);if(due!=INT64_MAX)k_work_reschedule(&locale_work,K_MSEC(MAX(1,due-k_uptime_get())));}
static void locale_work_run(struct k_work *work){
    k_mutex_lock(&state_mutex,K_FOREVER);
    locale_runtime.guarded=state.pressed!=0;
    ksl_tick(&locale_runtime,state.active.locales,k_uptime_get(),zmk_hid_get_explicit_mods(),(host_locks&2)!=0,&locale_io);
    locale_schedule();k_mutex_unlock(&state_mutex);
}
static void repeat_cancel(void);
static void locale_cancel(void){repeat_cancel();ksl_cancel(&locale_runtime,&locale_io);k_work_cancel_delayable(&locale_work);}
static void locale_endpoint(void);
static void locale_os_endpoint(bool usb);
#if IS_ENABLED(CONFIG_ZMK_DISPLAY)
static void locale_display_refresh(struct k_work *work) {
    (void)work;
    raise_zmk_layer_state_changed((struct zmk_layer_state_changed){.layer=zmk_keymap_highest_layer_active(),.state=true,.timestamp=k_uptime_get()});
}
K_WORK_DELAYABLE_DEFINE(locale_display_work, locale_display_refresh);
#endif
// The widget holds its own mutex. Never wait here for the input/configuration
// mutex: input can emit layer events while holding it. Retry after contention.
void ks_display_layer_name(unsigned layer, char *out, size_t size) {
    if (!size) return;
    out[0] = 0;
    if (k_mutex_lock(&state_mutex, K_NO_WAIT)) {
#if IS_ENABLED(CONFIG_ZMK_DISPLAY)
        k_work_reschedule(&locale_display_work, K_MSEC(20));
#endif
        return;
    }
    ksl_layer_name(state.active.locales, layer, out, size);
    k_mutex_unlock(&state_mutex);
}
#if IS_ENABLED(CONFIG_ZMK_DISPLAY)
#include "display_event.h"
ZMK_EVENT_IMPL(ks_display_key_changed);
static uint16_t display_keycode;
static unsigned display_computer;
static bool display_has_key;
static void display_key_refresh(struct k_work *work){(void)work;raise_ks_display_key_changed((struct ks_display_key_changed){.sequence=0});}
K_WORK_DELAYABLE_DEFINE(display_key_work,display_key_refresh);
static void display_key_pressed(uint16_t code){
    display_keycode=code;display_computer=locale_runtime.active;display_has_key=true;
    k_work_reschedule(&display_key_work,K_MSEC(20));
}
bool ks_display_last_key(char *out,size_t size){
    if(!size)return false;
    out[0]=0;
    if(k_mutex_lock(&state_mutex,K_NO_WAIT)){k_work_reschedule(&display_key_work,K_MSEC(20));return false;}
    if(display_has_key){
        uint16_t actions[5];const uint16_t *dance=NULL;
        if(display_keycode>=KD_BASE&&display_keycode<KD_BASE+KD_COUNT){memcpy(actions,&state.active.dances[display_keycode-KD_BASE],sizeof(actions));dance=actions;}
        ksl_key_label(state.active.locales,display_computer,display_keycode,dance,out,size);
    }
    k_mutex_unlock(&state_mutex);return true;
}
#else
static void display_key_pressed(uint16_t code){(void)code;}
#endif
static bool locale_initialized;
static void locale_load(bool preserve_typing){
    unsigned active=locale_runtime.active;bool initialized=locale_initialized;
    locale_cancel();
    if(preserve_typing&&initialized)ksl_runtime_reload(&locale_runtime,state.active.locales);
    else ksl_runtime_init(&locale_runtime,state.active.locales);
    locale_initialized=true;
#if IS_ENABLED(CONFIG_ZMK_DISPLAY)
    k_work_reschedule(&locale_display_work, K_NO_WAIT);
    if(display_has_key)k_work_reschedule(&display_key_work,K_MSEC(20));
#endif
    if(initialized&&active<state.active.locales[4])locale_runtime.active=active;
    locale_endpoint();
}
static void locale_endpoint(void){
    locale_cancel();locale_runtime.guarded=state.pressed!=0;struct zmk_endpoint_instance endpoint=zmk_endpoints_selected();
    locale_os_endpoint(endpoint.transport==ZMK_TRANSPORT_USB);
    if(endpoint.transport==ZMK_TRANSPORT_BLE)ksl_select(&locale_runtime,state.active.locales,ksl_bluetooth(state.active.locales,endpoint.ble.profile_index,locale_runtime.active));
}
static bool locale_event(uint16_t code,struct zmk_behavior_binding_event event,bool down){
    if(locale_replaying)return false;
    locale_runtime.guarded=state.pressed!=0;
    if(locale_bypass&&!ksl_supported(code))return ksl_extended_event(&locale_runtime,state.active.locales,event.position,code,down,event.timestamp,locale_mods(NULL),(host_locks&2)!=0,&locale_io);
    bool macro=event.position>=KS_KEYS+KS_ENCODERS+KS_COMBOS+512;
    if(ksl_character(code)&&!macro&&!ks_locale_override_process(code,event.layer,down))return true;
    bool handled=ksl_event(&locale_runtime,state.active.locales,event.position,code,down,event.timestamp,zmk_hid_get_explicit_mods(),(host_locks&2)!=0,&locale_io);
    locale_schedule();return handled;
}
static void locale_request(const uint8_t *in,uint8_t *out){
    memset(out,0,32);memcpy(out,in,5);
    if(in[5]||in[6]||(in[7]!=5&&in[7]!=6&&in[7]!=9)){out[5]=1;return;}
    locale_runtime.guarded=state.pressed!=0;
    if(in[7]==9){for(unsigned i=8;i<32;i++)if(in[i]){out[5]=1;return;}memcpy(out+6,state.active.locales+KSL_CRC_AT,4);memcpy(out+10,state.active.lighting,6);return;}
    if(in[7]==6){
        bool usb=zmk_endpoints_selected().transport==ZMK_TRANSPORT_USB;
        if(!ksl_usb_select(&locale_runtime,state.active.locales,in+8,out+6,usb,!state.unlocking))out[5]=1;
        locale_schedule();return;
    }
    if(in[8]!=255&&(state.unlocking||!ksl_select(&locale_runtime,state.active.locales,in[8]))){out[5]=1;return;}
    out[6]=locale_runtime.active;out[7]=locale_runtime.switching;
    locale_schedule();
}

void ks_locale_override(uint16_t code,bool down){
    if(!down)for(unsigned i=0;i<KSL_HELD;i++)if(locale_runtime.held[i].used&&locale_runtime.held[i].code==code)ksl_event(&locale_runtime,state.active.locales,locale_runtime.held[i].position,code,false,k_uptime_get(),locale_mods(NULL),(host_locks&2)!=0,&locale_io);
    ksl_event(&locale_runtime,state.active.locales,0x20000U+code,code,down,k_uptime_get(),locale_mods(NULL),(host_locks&2)!=0,&locale_io);locale_schedule();
}

#include "locales/os_port.inc"
