#include "pointer/pointer.h"
#include <zmk/hid.h>
#include <zmk/endpoints.h>
static struct ksp_state pointer_state;
static uint8_t pointer_mods[8];
static void pointer_send(void *ctx,struct ksp_report r){
    (void)ctx;zmk_hid_mouse_clear();zmk_hid_mouse_buttons_press(r.buttons);
    zmk_hid_mouse_movement_set(r.x,r.y);zmk_hid_mouse_scroll_set(r.h,r.v);
    zmk_endpoints_send_mouse_report();
}
static void pointer_work_run(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(pointer_work,pointer_work_run);
static void pointer_schedule(void){
    bool active=false;for(unsigned i=0;i<16;i++)if(i<4||i>=12)active|=pointer_state.held[i]!=0;
    if(active)k_work_reschedule(&pointer_work,K_MSEC(10));
}
static void pointer_work_run(struct k_work *work){
    (void)work;k_mutex_lock(&state_mutex,K_FOREVER);
    ksp_tick(&pointer_state,k_uptime_get(),pointer_send,NULL);pointer_schedule();
    k_mutex_unlock(&state_mutex);
}
static void pointer_cancel(void){
 k_work_cancel_delayable(&pointer_work);ksp_clear(&pointer_state,pointer_send,NULL);
 for(unsigned i=0;i<8;i++)while(pointer_mods[i]){pointer_mods[i]--;zmk_hid_unregister_mods(1u<<i);}
 zmk_endpoints_send_report(0x07);
}
static bool pointer_key(uint16_t c,bool down){
    if(c>0x1fff||(c&255)<0xcd||(c&255)>0xdf)return false;
    uint8_t mods=(c>>8)&15;if(c&0x1000)mods<<=4;
    if(mods){
      for(unsigned i=0;i<8;i++)if(mods&(1u<<i)){
        if(down&&pointer_mods[i]<255){pointer_mods[i]++;zmk_hid_register_mods(1u<<i);}
        else if(!down&&pointer_mods[i]){pointer_mods[i]--;zmk_hid_unregister_mods(1u<<i);}
      }
      zmk_endpoints_send_report(0x07);
    }
    ksp_key(&pointer_state,c&255,down,k_uptime_get(),pointer_send,NULL);
    pointer_schedule();return true;
}

bool ks_pointer_report(uint16_t c,bool down){return pointer_key(c,down);}
