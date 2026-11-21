/* Included at the end of the pinned HID implementation, Studio central only. */
#include <zmk/endpoints.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
static bool ks_nkro_track(zmk_key_t usage, bool down) {
    if (usage < 4 || usage >= KS_NKRO_BYTES * 8 || (usage >= 0xe0 && usage <= 0xe7)) return false;
    uint8_t mask=1u<<(usage%8);
    bool prior=(ks_nkro_bits[usage/8]&mask)!=0;
    if (down) ks_nkro_bits[usage/8]|=mask;
    else ks_nkro_bits[usage/8]&=~mask;
    return prior!=down;
}
static bool ks_nkro_held(zmk_key_t usage) {
    return usage<KS_NKRO_BYTES*8 && (ks_nkro_bits[usage/8]&(1u<<(usage%8)));
}
bool ks_nkro_enabled(void) { return ks_rollover.active; }
void ks_nkro_action(uint16_t code,bool down) {
    if (ksn_action(&ks_rollover,code,down) && down) zmk_endpoints_send_report(HID_USAGE_KEY);
}
struct ks_nkro_report *ks_nkro_get_report(void) {
    static struct ks_nkro_report report;
    memset(&report,0,sizeof(report)); report.id=KS_NKRO_REPORT_ID;
    if (ks_rollover.active) {
        report.mods=keyboard_report.body.modifiers;
        memcpy(report.bits,ks_nkro_bits,sizeof(report.bits));
    }
    return &report;
}
void ks_nkro_snapshot(struct ks_keyboard_packet *packet) {
    memset(packet,0,sizeof(*packet));
    if (ks_rollover.active) {
        packet->len=sizeof(struct ks_nkro_report);
        memcpy(packet->data,ks_nkro_get_report(),packet->len);
    } else {
        packet->len=sizeof(keyboard_report);
        memcpy(packet->data,&keyboard_report,packet->len);
    }
}
void ks_nkro_sent(int result) {
    if (result<0 || keyboard_report.body.modifiers || explicit_modifiers || implicit_modifiers) return;
    for (unsigned i=0;i<sizeof(ks_nkro_bits);i++) if(ks_nkro_bits[i])return;
    ksn_commit(&ks_rollover,true);
}
