// Included by the Studio adapter after translate(), under its existing mutex.
#include "locales/locale_runtime.h"
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <zmk/events/keycode_state_changed.h>
extern void ks_override_locale_tap(uint8_t,uint8_t);
static struct ksl_runtime locale_runtime;
static unsigned locale_bypass;
static bool locale_replaying;
static void locale_replay(void *ctx,unsigned pos,uint16_t code,bool down,int64_t time){
    (void)ctx;struct zmk_behavior_binding_event e={.position=pos,.timestamp=time,.layer=0,.source=ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL};
    bool previous=locale_replaying;locale_replaying=true;execute(code,e,down);locale_replaying=previous;
}
static void locale_action(void *ctx,uint16_t code,bool down){
    (void)ctx;
    struct zmk_behavior_binding_event e={.position=KS_KEYS+KS_ENCODERS+KS_COMBOS+1024+code,.timestamp=k_uptime_get(),.layer=0,.source=ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL};
    locale_bypass++;execute(code,e,down);locale_bypass--;
}
static void locale_pause(void *ctx){(void)ctx;k_msleep(5);}
static void locale_tap(void *ctx,uint8_t key,uint8_t mods){(void)ctx;ks_override_locale_tap(key,mods);}
extern uint8_t ks_locale_mods(void);
extern bool ks_locale_override_process(uint16_t,uint8_t,bool);
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
static const struct ksl_io locale_io={.tap=locale_tap,.action=locale_action,.mods=locale_mods,.prepare=locale_prepare,.pause=locale_pause,.replay=locale_replay,.hand=locale_hand};
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
static bool locale_initialized;
static void locale_load(void){unsigned active=locale_runtime.active;bool initialized=locale_initialized;locale_cancel();ksl_runtime_init(&locale_runtime,state.active.locales);locale_initialized=true;if(initialized&&active<state.active.locales[4])locale_runtime.active=active;locale_endpoint();}
static void locale_endpoint(void){
    locale_cancel();locale_runtime.guarded=state.pressed!=0;struct zmk_endpoint_instance endpoint=zmk_endpoints_selected();
    locale_os_endpoint(endpoint.transport==ZMK_TRANSPORT_USB);
    if(endpoint.transport==ZMK_TRANSPORT_BLE)ksl_select(&locale_runtime,state.active.locales,ksl_bluetooth(state.active.locales,endpoint.ble.profile_index,locale_runtime.active));
}
static bool locale_event(uint16_t code,struct zmk_behavior_binding_event event,bool down){
    if(locale_replaying)return false;
    locale_runtime.guarded=state.pressed!=0;
    if(locale_bypass&&!ksl_supported(code))return false;
    bool macro=event.position>=KS_KEYS+KS_ENCODERS+KS_COMBOS+512;
    if(ksl_character(code)&&!macro&&!ks_locale_override_process(code,event.layer,down))return true;
    bool handled=ksl_event(&locale_runtime,state.active.locales,event.position,code,down,event.timestamp,zmk_hid_get_explicit_mods(),(host_locks&2)!=0,&locale_io);
    locale_schedule();return handled;
}
static void locale_request(const uint8_t *in,uint8_t *out){
    memset(out,0,32);memcpy(out,in,5);
    if(in[5]||in[6]||(in[7]!=5&&in[7]!=6)){out[5]=1;return;}
    locale_runtime.guarded=state.pressed!=0;
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
