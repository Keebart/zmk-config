#pragma once
#include <stdbool.h>
#include <stdint.h>
#define KS_OVERRIDE_COUNT 32
struct ks_override {uint16_t trigger,replacement,layers;uint8_t trigger_mods,negative_mod_mask,suppressed_mods,options;};
_Static_assert(sizeof(struct ks_override)==10,"Vial override ABI");
struct ks_override_io {uint8_t (*mods)(void);void (*press)(uint8_t);void (*release)(uint8_t);void (*send)(void);void (*weak)(uint8_t);void (*mask)(uint8_t);void (*shortcut)(uint8_t);uint32_t (*clock)(void);void (*wait)(unsigned);void (*character)(uint16_t,bool);};
void ks_override_init(const struct ks_override *,const struct ks_override_io *);
bool ks_override_process(uint16_t,uint8_t,bool);
void ks_override_tick(void);
bool ks_override_pending(void);
void ks_override_cancel(void);
bool ks_override_valid(const struct ks_override *);
void ks_override_report(uint16_t,bool);
