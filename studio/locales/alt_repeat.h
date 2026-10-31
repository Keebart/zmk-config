#pragma once
#include "locale.h"
#define KAR_COUNT 32
#define KAR_REPEAT 0x7c79
#define KAR_ALT 0x7c7a
#define KAR_HELD 8
struct kar_rule { uint16_t keycode,alt_keycode; uint8_t allowed_mods,options; };
_Static_assert(sizeof(struct kar_rule)==6,"Vial Alt Repeat ABI");
struct kar_press { unsigned position; uint16_t code; uint8_t mods; bool used; };
struct kar_state { uint16_t last; uint8_t mods; bool replaying; struct kar_press held[KAR_HELD]; };
typedef void (*kar_emit)(void *,unsigned,uint16_t,uint8_t,bool);
bool kar_valid(const struct kar_rule *);
bool kar_allowed(uint16_t);
void kar_note(struct kar_state *,uint16_t,uint8_t);
uint16_t kar_alternate(const struct kar_rule *,uint16_t,uint8_t,uint8_t *);
bool kar_event(struct kar_state *,const struct kar_rule *,unsigned,uint16_t,bool,kar_emit,void *);
void kar_cancel(struct kar_state *,kar_emit,void *);
uint8_t kar_mods(const struct kar_state *);
