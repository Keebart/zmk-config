/* Studio-only QMK override/typing adapter. ZMK event/HID integration follows its MIT-licensed
 * hid_listener.c; the override and typing engines retain their original QMK GPL headers. */
#include "core.h"
#include "consumer_keys.h"
#include "override_bridge.h"
#include "word_bridge.h"
#include <limits.h>
#include <zephyr/kernel.h>
#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
extern struct k_mutex state_mutex;
extern const struct ks_override *ks_override_entries(void);
static uint8_t source_layer, normal_weak, override_weak, suppressed, repeat_weak, word_weak;
static bool initialized, consumer_dirty, sequence_active;
static uint8_t sequence_mods;
static unsigned bypass;
static uint32_t clock_ms(void){return (uint32_t)k_uptime_get();}
static void wait_time(unsigned n){k_msleep(n);}
static uint32_t usage(uint8_t code){return ks_key_usage(code);}
static void press(uint8_t code){zmk_hid_press(usage(code));if(ks_consumer_usage(code))consumer_dirty=true;}
static void release(uint8_t code){zmk_hid_release(usage(code));if(ks_consumer_usage(code))consumer_dirty=true;}
extern uint8_t ks_locale_report_mods(uint8_t);
extern uint8_t ks_locale_report_mask(void);
static void update_mods(void){
    if(sequence_active){zmk_hid_masked_modifiers_set(255);zmk_hid_implicit_modifiers_press(sequence_mods);return;}
    zmk_hid_masked_modifiers_set(suppressed|ks_locale_report_mask());
    zmk_hid_implicit_modifiers_press(ks_locale_report_mods(((normal_weak|repeat_weak|word_weak)&~suppressed)|override_weak));
}
static void send(void){update_mods();zmk_endpoints_send_report(HID_USAGE_KEY);if(consumer_dirty){consumer_dirty=false;zmk_endpoints_send_report(HID_USAGE_CONSUMER);}}
static void weak(uint8_t mods){override_weak=mods;update_mods();}
static void mask(uint8_t mods){suppressed=mods;update_mods();}
static void shortcut(uint8_t m){normal_weak|=m;update_mods();}
extern void ks_locale_override(uint16_t,bool);
static const struct ks_override_io io={.mods=zmk_hid_get_explicit_mods,.press=press,.release=release,.send=send,.weak=weak,.mask=mask,.shortcut=shortcut,.clock=clock_ms,.wait=wait_time,.character=ks_locale_override};
void ks_override_prepare(void){if(!initialized){initialized=true;ks_override_init(ks_override_entries(),&io);}}
static void tick(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(override_work,tick);
void ks_word_schedule(void){
    int64_t due=ks_override_pending()?k_uptime_get()+1:ks_word_deadline();
    if(due!=INT64_MAX)k_work_reschedule(&override_work,K_MSEC(MAX(1,due-k_uptime_get())));
}
static void tick(struct k_work *work){
    k_mutex_lock(&state_mutex,K_FOREVER);ks_override_tick();ks_word_tick();ks_word_schedule();k_mutex_unlock(&state_mutex);
}
void ks_override_lifecycle_cancel(void){
    ks_word_cancel();
    if(initialized){ks_override_cancel();k_work_cancel_delayable(&override_work);}
}
void add_mods(uint8_t m){zmk_hid_register_mods(m);}
void del_mods(uint8_t m){zmk_hid_unregister_mods(m);}
void clear_mods(void){zmk_hid_unregister_mods(255);}
uint8_t get_weak_mods(void){return word_weak;}
void add_weak_mods(uint8_t m){word_weak|=m;update_mods();}
void del_weak_mods(uint8_t m){word_weak&=~m;update_mods();}
void clear_weak_mods(void){word_weak=0;update_mods();}
void unregister_weak_mods(uint8_t m){word_weak&=~m;send();}
void ks_word_begin(void){word_weak=0;update_mods();}
void ks_word_action(uint16_t code,bool down){ks_override_prepare();ks_word_process(code,down);ks_word_schedule();}
// Observe the originating layer of resolved hold/tap, sticky and physical actions.
// Wrapping only this Studio build avoids changing upstream ZMK behavior sources.
extern int __real_zmk_behavior_invoke_binding(const struct zmk_behavior_binding *,struct zmk_behavior_binding_event,bool);
int __wrap_zmk_behavior_invoke_binding(const struct zmk_behavior_binding *binding,struct zmk_behavior_binding_event event,bool down){
    k_mutex_lock(&state_mutex,K_FOREVER);
    uint8_t previous=source_layer;source_layer=event.layer<16?event.layer:0;
    // QMK macro bytecode injects reports directly, outside process_key_override.
    bool macro=event.position>=KS_KEYS+KS_ENCODERS+KS_COMBOS+512 && event.position<KS_KEYS+KS_ENCODERS+KS_COMBOS+768;
    if(macro)bypass++;
    int result=__real_zmk_behavior_invoke_binding(binding,event,down);
    if(macro)bypass--;
    source_layer=previous;k_mutex_unlock(&state_mutex);return result;
}
static uint16_t qmk_code(const struct zmk_keycode_state_changed *e){
    uint16_t c;
    if(e->usage_page==HID_USAGE_CONSUMER){
        c=ks_consumer_code(e->keycode);
        if(c==0xffff)return c;
    } else {
        if(e->usage_page!=HID_USAGE_KEY || e->keycode>255)return 0xffff;
        c=e->keycode;
    }
    uint8_t mods=e->implicit_modifiers;
    if(mods && !(mods&0xf0))c|=(uint16_t)mods<<8;
    else if(mods && !(mods&15))c|=0x1000|((uint16_t)mods<<4);
    return c;
}
static int key_event(const zmk_event_t *event){
    const struct zmk_keycode_state_changed *e=as_zmk_keycode_state_changed(event);
    if(!e || e->usage_page==0xff60)return ZMK_EV_EVENT_BUBBLE;
    k_mutex_lock(&state_mutex,K_FOREVER);ks_override_prepare();
    uint16_t code=qmk_code(e);
    extern bool ks_pointer_report(uint16_t,bool);
    if(e->usage_page==HID_USAGE_KEY && ks_pointer_report(code,e->state)){k_mutex_unlock(&state_mutex);return ZMK_EV_EVENT_BUBBLE;}
    extern void ks_repeat_note(uint16_t,uint8_t);
    if(e->state&&code!=0xffff)ks_repeat_note(code,zmk_hid_get_explicit_mods()|e->implicit_modifiers);
    bool pass=true;
    if(!bypass && code!=0xffff){
        if(e->state)ks_word_begin();
        pass=ks_override_process(code,source_layer,e->state);
        if(pass)pass=ks_word_process(code,e->state);
        if(pass)pass=ks_auto_shift_process(code,e->state);
        ks_word_schedule();
    }
    if(pass){
        if(e->usage_page==HID_USAGE_CONSUMER)consumer_dirty=true;
        if(e->state){
            if(!is_mod(e->usage_page,e->keycode) && zmk_hid_is_pressed(ZMK_HID_USAGE(e->usage_page,e->keycode))){zmk_hid_release(ZMK_HID_USAGE(e->usage_page,e->keycode));send();}
            zmk_hid_press(ZMK_HID_USAGE(e->usage_page,e->keycode));zmk_hid_register_mods(e->explicit_modifiers);normal_weak=e->implicit_modifiers;
        }else{
            zmk_hid_release(ZMK_HID_USAGE(e->usage_page,e->keycode));
#if IS_ENABLED(CONFIG_ZMK_HID_SEPARATE_MOD_RELEASE_REPORT)
            send();
#endif
            zmk_hid_unregister_mods(e->explicit_modifiers);normal_weak=0;
        }
        send();
    }
    k_mutex_unlock(&state_mutex);return ZMK_EV_EVENT_BUBBLE;
}
void ks_override_report(uint16_t code,bool down){
    k_mutex_lock(&state_mutex,K_FOREVER);ks_override_prepare();ks_override_process(code,source_layer,down);ks_word_schedule();k_mutex_unlock(&state_mutex);
}
ZMK_LISTENER(keebart_override_hid,key_event);
ZMK_SUBSCRIPTION(keebart_override_hid,zmk_keycode_state_changed);

void ks_override_locale_tap(uint8_t key,uint8_t mods){
    zmk_hid_masked_modifiers_set(255);zmk_hid_implicit_modifiers_press(mods);
    extern uint16_t ks_locale_tap_delay(uint16_t);
    zmk_hid_press(HID_USAGE_KEY<<16|key);zmk_endpoints_send_report(HID_USAGE_KEY);uint16_t delay=ks_locale_tap_delay(key);if(delay)k_msleep(delay);
    zmk_hid_release(HID_USAGE_KEY<<16|key);zmk_endpoints_send_report(HID_USAGE_KEY);
    update_mods();zmk_endpoints_send_report(HID_USAGE_KEY);
}

uint8_t ks_locale_mods(void){return ((zmk_hid_get_explicit_mods()|normal_weak|repeat_weak|word_weak)&~suppressed)|override_weak;}
bool ks_locale_override_process(uint16_t code,uint8_t layer,bool down){ks_override_prepare();bool pass=ks_override_process(code,layer,down);ks_word_schedule();return pass;}

void ks_repeat_weak(uint8_t mods){repeat_weak=mods;send();}

void ks_override_sequence(uint8_t key,uint8_t mods,bool end){
    sequence_active=!end;sequence_mods=mods;update_mods();zmk_endpoints_send_report(HID_USAGE_KEY);
    if(!end&&key){zmk_hid_press(HID_USAGE_KEY<<16|key);zmk_endpoints_send_report(HID_USAGE_KEY);k_msleep(5);zmk_hid_release(HID_USAGE_KEY<<16|key);zmk_endpoints_send_report(HID_USAGE_KEY);}
}

void ks_locale_stroke(uint8_t key,bool down){
    if(key){if(down)zmk_hid_press(HID_USAGE_KEY<<16|key);else zmk_hid_release(HID_USAGE_KEY<<16|key);}
    send();
}
