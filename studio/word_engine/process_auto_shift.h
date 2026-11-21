#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "action.h"
#include "keyboard.h"
#include "keycodes.h"
#ifndef AUTO_SHIFT_TIMEOUT
#define AUTO_SHIFT_TIMEOUT 175
#endif
#define IS_RETRO(kc) (IS_QK_MOD_TAP(kc) || IS_QK_LAYER_TAP(kc))
#define DO_GET_AUTOSHIFT_TIMEOUT(keycode, record, ...) record
#define AUTO_SHIFT_ALPHA KC_A ... KC_Z
#define AUTO_SHIFT_NUMERIC KC_1 ... KC_0
#define AUTO_SHIFT_SYMBOLS KC_MINUS ... KC_SLASH: case KC_NONUS_BACKSLASH
#define AUTO_SHIFT_SPECIAL KC_TAB: case AUTO_SHIFT_SYMBOLS
bool process_auto_shift(uint16_t keycode, keyrecord_t *record);
void retroshift_poll_time(keyevent_t *event);
void retroshift_swap_times(void);
void autoshift_enable(void);
void autoshift_disable(void);
void autoshift_toggle(void);
bool get_autoshift_state(void);
uint16_t get_generic_autoshift_timeout(void);
uint16_t (get_autoshift_timeout)(uint16_t keycode, keyrecord_t *record);
void set_autoshift_timeout(uint16_t timeout);
void autoshift_matrix_scan(void);
bool get_custom_auto_shifted_key(uint16_t keycode, keyrecord_t *record);
bool get_auto_shifted_key(uint16_t keycode, keyrecord_t *record);
bool get_auto_shift_repeat(uint16_t keycode, keyrecord_t *record);
bool get_auto_shift_no_auto_repeat(uint16_t keycode, keyrecord_t *record);
void autoshift_press_user(uint16_t keycode, bool shifted, keyrecord_t *record);
void autoshift_release_user(uint16_t keycode, bool shifted, keyrecord_t *record);
void ks_autoshift_cancel(void);
bool ks_autoshift_pending(void);
