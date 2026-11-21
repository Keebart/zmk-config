// Explicit host-only reset. Never erase identities or the split bond namespace.
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/settings/settings.h>
#include <zmk/ble.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
extern void set_profile_address(uint8_t index, const bt_addr_le_t *addr);
static uint32_t completed_generation;
static uint8_t completed_result;
static bool clear_hosts(void) {
    for (unsigned i=0;i<ZMK_BLE_PROFILE_COUNT;i++) {
        const bt_addr_le_t address=*zmk_ble_profile_address(i);
        // Passing ANY/NULL to bt_unpair would clear *all* bonds, including split.
        if (bt_addr_le_cmp(&address,BT_ADDR_LE_ANY) && bt_unpair(BT_ID_DEFAULT,&address))return false;
        set_profile_address(i,BT_ADDR_LE_ANY);
        // Upstream's address setter does not return storage errors. Explicitly
        // persist and check the cleared host record before reporting success.
        char key[24];snprintf(key,sizeof(key),"ble/profiles/%u",i);
        struct zmk_ble_profile empty={0};
        if(settings_save_one(key,&empty,sizeof(empty)))return false;
    }
    if(zmk_ble_prof_select(0))return false;
    const uint8_t first=0;
    return settings_save_one("ble/active_profile",&first,sizeof(first))==0;
}
bool ks_host_reset(uint32_t generation) {
    completed_generation=generation;
    completed_result=clear_hosts()?1:2;
    return completed_result==1;
}
void ks_host_reset_status(const uint8_t in[32],uint8_t out[32]) {
    memset(out,0,32);memcpy(out,in,5);
    if(in[5]){out[5]=2;return;}
    for(unsigned i=6;i<32;i++)if(in[i]){out[5]=2;return;}
    for(unsigned i=0;i<4;i++)out[6+i]=completed_generation>>(8*i);
    out[10]=completed_result;
}
