#pragma once
#include "locale.h"
#define KSPW_SAVE_MS 3000
#define KSPW_RECORD_BYTES 8
struct kspw_state {uint32_t base, changed; bool dirty;};
// Six formerly reserved rule-padding bytes; record length and addresses stay fixed.
void kspw_options(const uint8_t *,uint8_t[6]);
uint8_t kspw_flags(const uint8_t *);
uint8_t kspw_dim(const uint8_t *);
uint32_t kspw_idle_ms(const uint8_t *);
uint32_t kspw_sleep_ms(const uint8_t *);
bool kspw_display_off(const uint8_t *);
uint8_t kspw_factor(const uint8_t *);
bool kspw_valid(const uint8_t[6]);
// Physical lighting actions never modify the layer palette or animation recipe.
bool kspw_key(uint16_t);
bool kspw_set(struct kspw_state *,uint8_t *,uint8_t,uint8_t,uint32_t);
bool kspw_action(struct kspw_state *,uint8_t *,uint16_t,bool,uint32_t);
void kspw_init(struct kspw_state *,uint8_t *,const uint8_t[KSPW_RECORD_BYTES],uint32_t);
// Anchor the overlay to a durable save generation, even when contents repeat.
void kspw_committed(struct kspw_state *,uint32_t);
bool kspw_due(const struct kspw_state *,uint32_t,bool);
void kspw_record(const struct kspw_state *,const uint8_t *,uint8_t[KSPW_RECORD_BYTES]);
