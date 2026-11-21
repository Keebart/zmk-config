#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "extended.h"
#define KSL_BYTES 3584
#define KSL_CRC_AT (KSL_BYTES-4)
#define KSL_BINDINGS 256
#define KSL_BINDING 0xd000
#define KSL_BINDING_AT 484
#define KSL_COMPUTERS 6
#define KSL_LAYER_RULES 6
#define KSL_LAYER_RULE_AT 3556
#define KSL_ACTIONS 64
#define KSL_PERMISSIVE 1
#define KSL_TAP_PREFERRED 2
#define KSL_RETRO 4
#define KSL_CHORDAL 8
#define KSL_SELECT 0x7d80
#define KSL_NEXT (KSL_SELECT+KSL_COMPUTERS)
#define KSL_PREVIOUS (KSL_NEXT+1)
#define KSL_COMPOUND 0xc000
struct ksl_stroke { uint8_t key, mods; };
struct ksl_output { struct ksl_stroke strokes[2]; uint32_t unicode; uint8_t count; };
uint32_t ksl_crc(const uint8_t *,size_t);
bool ksl_valid(const uint8_t *);
bool ksl_legacy_valid(const uint8_t *);
bool ksl_upgrade(uint8_t *,const uint8_t *);
uint16_t ksl_binding(const uint8_t *,unsigned,uint16_t);
void ksl_default(uint8_t *);
void ksl_stock(uint8_t *);
uint16_t ksl_stock_code(uint16_t);
static inline bool ksl_character(uint16_t code){return (code>=0x8020&&code<=0xafff&&!(code>=0x807f&&code<=0x809f))||(code>=0xb001&&code<=0xb01f)||(code>=0xb020&&code<=0xbfff&&!(code>=0xb07f&&code<=0xb09f));}
bool ksl_supported(uint16_t);
bool ksl_compound_key(uint16_t);
bool ksl_resolve(const uint8_t *,unsigned,uint16_t,uint8_t,bool,struct ksl_output *);
unsigned ksl_bluetooth(const uint8_t *,unsigned,unsigned);
uint16_t ksl_compound(const uint8_t *,uint16_t,unsigned);

unsigned ksl_os_computer(const uint8_t *,unsigned);

bool ksl_rgb(const uint8_t *,uint16_t,uint16_t[3]);

bool ksl_chordal_exempt(const uint8_t *,unsigned);

unsigned ksl_lighting_idle_seconds(const uint8_t *);
bool ksl_lighting_awake(const uint8_t *,uint32_t,bool);

const uint8_t *ksl_typing(const uint8_t *);

static inline bool ksl_auto_shift_key(const uint8_t *record,uint16_t code){
 const uint8_t *t=ksl_typing(record);
 return ((t[0]&1)&&code>=4&&code<=29)||((t[0]&2)&&code>=30&&code<=39)||
        ((t[0]&4)&&((code>=45&&code<=56)||code==100))||((t[0]&8)&&code==43);
}

uint16_t ksl_shifted_character(const uint8_t *,unsigned,uint16_t);
bool ksl_letter(const uint8_t *,unsigned,uint16_t);
bool ksl_auto_shift_character(const uint8_t *,unsigned,uint16_t);

// Returns the full UTF-8 name in a caller-owned buffer, or empty for unnamed layers.
void ksl_layer_name(const uint8_t *, unsigned, char *, size_t);

void ksl_macro_name(const uint8_t *, unsigned, char *, size_t);
void ksl_key_label(const uint8_t *,unsigned,uint16_t,const uint16_t *,char *,size_t);

unsigned ksl_typing_size(const uint8_t *p);
uint16_t ksl_tap_delay(const uint8_t *p,uint16_t code);
uint16_t ksl_hold_term(const uint8_t *p);
uint8_t ksl_tapping_toggle(const uint8_t *p);

/* KSL3 packs only configured computer columns; legacy EEPROM offsets stay fixed. */
unsigned ksl_compound_at(unsigned);
unsigned ksl_binding_at(const uint8_t *);
unsigned ksl_binding_stride(const uint8_t *);
unsigned ksl_prefix_end(const uint8_t *);
bool ksl_extra_combo(const uint8_t *,unsigned,uint16_t[5]);
