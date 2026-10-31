// Passive USB callbacks only collect evidence. Selection runs under the central's mutex.
#include "os_detection.h"
static struct kso_capture os_capture;
static struct k_spinlock os_lock;
static uint32_t os_applied_epoch=UINT32_MAX;
static bool os_applied_usb;
static void os_work_run(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(os_work,os_work_run);
void keebart_os_descriptor(uint16_t length){
    k_spinlock_key_t key=k_spin_lock(&os_lock);
    kso_observe(&os_capture,length,k_uptime_get_32());
    k_spin_unlock(&os_lock,key);
    k_work_reschedule(&os_work,K_MSEC(500));
}
void keebart_os_usb(bool configured){
    k_spinlock_key_t key=k_spin_lock(&os_lock);
    if(configured)kso_configure(&os_capture,k_uptime_get_32());else kso_reset(&os_capture);
    k_spin_unlock(&os_lock,key);
    if(configured)k_work_reschedule(&os_work,K_MSEC(500));
}
static void locale_os_endpoint(bool usb){
    if(!usb)os_applied_usb=false;
    else k_work_reschedule(&os_work,K_MSEC(500));
}
static void os_work_run(struct k_work *work){
    (void)work;
    k_mutex_lock(&state_mutex,K_FOREVER);
    k_spinlock_key_t key=k_spin_lock(&os_lock);
    bool ready=kso_ready(&os_capture,k_uptime_get_32()),configured=os_capture.configured;
    unsigned os=os_capture.os;uint32_t epoch=os_capture.epoch;
    k_spin_unlock(&os_lock,key);
    if(!ready||!locale_initialized||state.unlocking){
        if(configured)k_work_reschedule(&os_work,K_MSEC(500));
    }else if(zmk_endpoints_selected().transport==ZMK_TRANSPORT_USB&&(!os_applied_usb||os_applied_epoch!=epoch)){
        os_applied_usb=true;os_applied_epoch=epoch;
        unsigned selected=ksl_os_computer(state.active.locales,os);
        locale_runtime.guarded=state.pressed!=0;
        if(selected!=255)ksl_select(&locale_runtime,state.active.locales,selected);
        locale_schedule();
    }
    k_mutex_unlock(&state_mutex);
}
