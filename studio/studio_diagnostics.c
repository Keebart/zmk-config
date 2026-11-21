#include "diagnostics/diagnostics.h"
#include "release.h"
#include "hardware.h"
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/fatal.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/arch/arm/exc.h>
#include <hal/nrf_power.h>
#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>
static __noinit struct kdg_store retained;
static struct kdg_runtime recorder;
static volatile uint32_t sampled_uptime;
void kdg_zmk_add(uint32_t kind,uint32_t value,uint32_t aux){uint32_t now=k_uptime_get_32();unsigned key=irq_lock();sampled_uptime=now;kdg_add(&recorder,now,kind,value,aux);irq_unlock(key);}
static int init(void){uint32_t reset=NRF_POWER->RESETREAS;NRF_POWER->RESETREAS=reset;kdg_init(&recorder,&retained,KEEBART_RELEASE_ID,IS_ENABLED(CONFIG_KEEBART_STUDIO)?1:2,reset);return 0;}
SYS_INIT(init,PRE_KERNEL_1,99);
void __real_z_fatal_error(unsigned int reason,const z_arch_esf_t *esf);
void __wrap_z_fatal_error(unsigned int reason,const z_arch_esf_t *esf){
    /* No allocation, flash writes, Bluetooth calls or changed recovery policy. */
    unsigned key=irq_lock();kdg_add(&recorder,sampled_uptime,KDG_FATAL,reason,esf?esf->basic.pc:0);
    if(esf)kdg_add(&recorder,sampled_uptime,KDG_PC,esf->basic.lr,esf->basic.xpsr);
    irq_unlock(key);__real_z_fatal_error(reason,esf);
}
static uint32_t role(struct bt_conn *c){struct bt_conn_info i;return bt_conn_get_info(c,&i)?255:i.role;}
static void connected(struct bt_conn *c,uint8_t error){kdg_zmk_add(KDG_BT_CONNECTED,error,role(c));}
static void disconnected(struct bt_conn *c,uint8_t reason){kdg_zmk_add(KDG_BT_DISCONNECTED,reason,role(c));}
static void security(struct bt_conn *c,bt_security_t level,enum bt_security_err err){kdg_zmk_add(KDG_BT_SECURITY,err,((uint32_t)role(c)<<8)|level);}
BT_CONN_CB_DEFINE(keebart_diagnostics)={.connected=connected,.disconnected=disconnected,.security_changed=security};
static int usb(const zmk_event_t *event){kdg_zmk_add(KDG_USB,zmk_usb_get_conn_state(),0);return ZMK_EV_EVENT_BUBBLE;}
ZMK_LISTENER(keebart_diagnostic_usb,usb);
ZMK_SUBSCRIPTION(keebart_diagnostic_usb,zmk_usb_conn_state_changed);
bool kdg_zmk_read(uint32_t index,uint8_t out[24]){uint32_t now=k_uptime_get_32();unsigned key=irq_lock();sampled_uptime=now;bool ok=kdg_read(&recorder,index,now,out);irq_unlock(key);return ok;}
/* One bounded peripheral summary is piggybacked on the existing startup release
 * read. No new periodic radio traffic; central retains it after disconnection. */
void kdg_zmk_summary(uint8_t out[72]){unsigned key=irq_lock();kdg_read(&recorder,KDG_BOOT_INDEX,0,out);kdg_read(&recorder,KDG_FATAL_INDEX,0,out+24);kdg_read(&recorder,recorder.latest,0,out+48);irq_unlock(key);}
#if IS_ENABLED(CONFIG_KEEBART_STUDIO)
static uint8_t peer[72];static bool peer_valid;
void kdg_zmk_peer(const uint8_t in[72]){unsigned key=irq_lock();memcpy(peer,in,72);peer_valid=true;irq_unlock(key);}
static bool peer_read(uint32_t index,uint8_t out[24]){
    unsigned key=irq_lock();bool ok=peer_valid;memset(out,0,24);
    if(ok){if(!index){memcpy(out,"KDG1",4);kdg_u32(out+4,KEEBART_RELEASE_ID);kdg_u32(out+8,kdg_get(peer+4));kdg_u32(out+12,kdg_get(peer+48));out[16]=2;out[17]=1;out[18]=2;out[19]=1;}
    else{const uint8_t *e=index==KDG_BOOT_INDEX?peer:index==KDG_FATAL_INDEX?peer+24:peer+48;ok=kdg_get(e)!=0&&(index>=KDG_BOOT_INDEX||kdg_get(e)==index);if(ok)memcpy(out,e,24);}}
    irq_unlock(key);return ok;
}
void kdg_zmk_handle(const uint8_t in[32],uint8_t out[32]){
    memset(out,0,32);memcpy(out,in,5);
    if(in[0]!='K'||in[1]!='S'||in[2]!=3||in[3]!=21||in[5]||in[6]>1){out[5]=2;return;}
    for(unsigned i=11;i<32;i++)if(in[i]){out[5]=2;return;}
    uint32_t index=kdg_get(in+7);if(!(in[6]?peer_read(index,out+6):kdg_zmk_read(index,out+6)))out[5]=4;
}
#endif
