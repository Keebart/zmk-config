#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#define VIAL_COMBO_ENABLE 1
#define VIAL_COMBO_ENTRIES 64
#define NO_ACTION_TAPPING 1
#define KEEBART_STUDIO_TAP_DANCE 1
#define COMBO_TERM kc_term()
#define COMBO_HOLD_TERM 200
#define QK_COMBO_ON 0x7c50
#define QK_COMBO_OFF 0x7c51
#define QK_COMBO_TOGGLE 0x7c52
#define layer_state 0u
#define default_layer_state 0u
#define KEY_EVENT 1
#define TICK_EVENT 0
#define COMBO_EVENT 2
#define IS_NOEVENT(e) ((e).type==TICK_EVENT)
typedef struct {uint8_t row,col;} keypos_t;
typedef struct {keypos_t key;bool pressed;uint8_t type;uint16_t time;} keyevent_t;
typedef struct {keyevent_t event;uint16_t keycode;} keyrecord_t;
#define MAKE_KEYPOS(r,c) ((keypos_t){r,c})
#define MAKE_COMBOEVENT(d) ((keyevent_t){.type=COMBO_EVENT,.pressed=d,.time=kc_clock()})
uint16_t kc_clock(void);
uint16_t kc_elapsed(uint16_t);
uint16_t kc_term(void);
void kc_record(keyrecord_t *);
uint16_t kc_trigger_code(const uint16_t *,uint16_t);
#define timer_read kc_clock
#define timer_elapsed kc_elapsed
#define kst_combo_code kc_trigger_code
#define process_record kc_record
static inline uint8_t get_highest_layer(uint32_t x){(void)x;return 0;}
static inline void clear_weak_mods(void){}
#define combo_count kc_combo_count
#define combo_get kc_combo_get
#define key_combos kc_key_combos
#define process_combo kc_process_combo
#define combo_task kc_combo_task
#define process_combo_event kc_process_combo_event
#define combo_ref_from_layer kc_combo_ref_from_layer
#define clear_combos kc_clear_combos
#define drop_combo_from_buffer kc_drop_combo_from_buffer
#define apply_combo kc_apply_combo
#define overlaps kc_overlaps
#define combo_enable kc_combo_enable
#define combo_disable kc_combo_disable
#define combo_toggle kc_combo_toggle
#define is_combo_enabled kc_is_combo_enabled

uint16_t keymap_key_to_keycode(uint8_t,keypos_t);
