#pragma once
#define KSE_UNICODE 0xc100
#define KSE_UNICODE_COUNT 128
#define KSE_LEADER 0x7c58
#define KSE_KEY_LOCK 0x7c59
#define KSE_SENTENCE 0x7f80
#define KSE_KEYBOARD_LOCK 0x7f81
#define KSE_LUMINO 0x7f82
#define KSE_JIGGLER 0x7f83
#define KSE_SWAP 0x56f0
static inline bool kse_code(uint16_t c){return (c>=0x7011&&c<=0x7013)||(c>=0x7f90&&c<=0x7f93)||(c>=KSE_UNICODE&&c<KSE_UNICODE+KSE_UNICODE_COUNT)||c==KSE_LEADER||c==KSE_KEY_LOCK||(c>=KSE_SENTENCE&&c<=KSE_JIGGLER)||c==KSE_SWAP||(c>=0x56f2&&c<=0x56f6);}
unsigned kse_size(const uint8_t *);
const uint8_t *kse_options(const uint8_t *);
bool kse_valid(const uint8_t *);
unsigned kse_swap_position(const uint8_t *,unsigned);
uint8_t kse_brightness(const uint8_t *,uint32_t,bool);

uint16_t kse_leader_tap(const uint8_t *,uint16_t);

unsigned kse_program(const uint8_t *,unsigned);
uint32_t kse_utf8(const uint8_t *,unsigned,unsigned *);
