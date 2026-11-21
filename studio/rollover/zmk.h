#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "rollover/rollover.h"
#define KS_NKRO_REPORT_ID 4
#define KS_NKRO_BYTES 30
struct ks_nkro_report { uint8_t id, mods, bits[KS_NKRO_BYTES]; } __attribute__((packed));
/* One tagged queue preserves order across a mode transition. */
struct ks_keyboard_packet { uint8_t len; uint8_t data[sizeof(struct ks_nkro_report)]; };
bool ks_nkro_enabled(void);
void ks_nkro_action(uint16_t code, bool down);
void ks_nkro_snapshot(struct ks_keyboard_packet *packet);
void ks_nkro_sent(int result);
struct ks_nkro_report *ks_nkro_get_report(void);
