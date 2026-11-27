/* Runtime power policy. All peripheral writes run outside Bluetooth callbacks. */
#include "locales/power_policy.h"
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <string.h>
static atomic_t idle_ms=300000,sleep_ms=3600000,display_blank=1,display_dark=0,light_factor=255;
uint32_t ks_power_idle_ms(void){return atomic_get(&idle_ms);}
uint32_t ks_power_sleep_ms(void){return atomic_get(&sleep_ms);}
bool ks_power_display_blank(void){return atomic_get(&display_blank);}
bool ks_power_display_dark(void){return atomic_get(&display_dark);}
uint8_t ks_power_factor(void){return atomic_get(&light_factor);}
__attribute__((weak)) void ks_mip_theme_changed(void){}
static void apply(const uint8_t p[6]){
 atomic_set(&idle_ms,kspw_policy_idle(p));atomic_set(&sleep_ms,kspw_policy_sleep(p));
 atomic_set(&display_blank,!(p[3]&1));atomic_set(&display_dark,kspw_policy_display_dark(p));
 atomic_set(&light_factor,kspw_policy_factor(p));
}
#if IS_ENABLED(CONFIG_KEEBART_STUDIO)
extern void ks_power_options(uint8_t out[6]);
extern void ks_power_flush(void);
void ks_power_sync_display_dark(bool dark){
 if(atomic_set(&display_dark,dark?1:0)!=(dark?1:0))ks_mip_theme_changed();
}
void ks_power_packet(uint8_t out[12]){
 out[0]='P';out[1]='W';out[2]=1;out[3]=6;ks_power_options(out+4);
 uint16_t crc=kspw_policy_crc(out,10);out[10]=crc;out[11]=crc>>8;
}
#else
static uint8_t saved[8];
K_MSGQ_DEFINE(power_updates,6,1,1);
static int load(const char *key,size_t len,settings_read_cb read,void *arg){
 if(strcmp(key,"policy")||len!=8||read(arg,saved,8)!=8)return -EINVAL;
 if(!kspw_policy_valid(saved)||kspw_policy_crc(saved,6)!=(saved[6]|(unsigned)saved[7]<<8))return -EINVAL;
 apply(saved);return 0;
}
SETTINGS_STATIC_HANDLER_DEFINE(keebart_power,"keebart_power",NULL,load,NULL,NULL);
bool ks_power_receive(const uint8_t *p,unsigned n){
 if(n!=12||memcmp(p,"PW\x01\x06",4)||!kspw_policy_valid(p+4)||kspw_policy_crc(p,10)!=(p[10]|(unsigned)p[11]<<8))return false;
 k_msgq_purge(&power_updates);return k_msgq_put(&power_updates,p+4,K_NO_WAIT)==0;
}
#endif
static void run(void *a,void *b,void *c){
 (void)a;(void)b;(void)c;uint8_t previous[6]={0};
#if !IS_ENABLED(CONFIG_KEEBART_STUDIO)
 memcpy(previous,saved,6);bool dirty=false;int64_t changed=0;
#endif
 for(;;){
  uint8_t p[6];
#if IS_ENABLED(CONFIG_KEEBART_STUDIO)
  ks_power_options(p);ks_power_flush();
#else
  if(k_msgq_get(&power_updates,p,K_MSEC(250)))memcpy(p,previous,6);
#endif
  if(kspw_policy_valid(p)&&memcmp(p,previous,6)){
   bool display_changed=(p[3]^previous[3])&1;
   bool theme_changed=(p[3]^previous[3])&4;
   apply(p);memcpy(previous,p,6);
#if !IS_ENABLED(CONFIG_KEEBART_STUDIO)
   dirty=true;changed=k_uptime_get();
#endif
   if(display_changed)raise_zmk_activity_state_changed((struct zmk_activity_state_changed){.state=zmk_activity_get_state()});
   if(theme_changed)ks_mip_theme_changed();
  }
#if !IS_ENABLED(CONFIG_KEEBART_STUDIO)
  if(dirty&&k_uptime_get()-changed>=3000){
   uint8_t record[8];memcpy(record,previous,6);uint16_t crc=kspw_policy_crc(record,6);record[6]=crc;record[7]=crc>>8;
   if(!settings_save_one("keebart_power/policy",record,8)){memcpy(saved,record,8);dirty=false;}else changed=k_uptime_get();
  }
#else
  k_sleep(K_MSEC(250));
#endif
 }
}
K_THREAD_DEFINE(keebart_power_thread,2048,run,NULL,NULL,NULL,9,0,1000);
