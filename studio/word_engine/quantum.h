#include "caps_word.h"
#pragma once
#include <string.h>
#include "action.h"
#include "action_util.h"
#include "quantum_keycodes.h"
#include "timer.h"
void register_code16(uint16_t);
void unregister_code16(uint16_t);
void wait_ms(unsigned);
static inline void qs_wait_ms(unsigned ms) { if (ms) wait_ms(ms); }
#define GET_TAPPING_TERM(kc, r) 200
#define send_string(s) ((void)(s))
#define get_u16_str(v, p) ""
