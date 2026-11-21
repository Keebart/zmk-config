#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint8_t row,col;} keypos_t;
typedef struct {keypos_t key;bool pressed;uint16_t time;} keyevent_t;
typedef struct {keyevent_t event;struct {uint8_t count;bool interrupted;} tap;} keyrecord_t;
#define NO_ACTION_ONESHOT
#define NO_ACTION_TAPPING
#define CAPS_WORD_ENABLE
#define AUTO_SHIFT_ENABLE
#define AUTO_SHIFT_MODIFIERS
#define AUTO_SHIFT_TIMEOUT_PER_KEY
#define AUTO_SHIFT_REPEAT_PER_KEY
#define AUTO_SHIFT_NO_AUTO_REPEAT_PER_KEY
#define AUTO_SHIFT_NO_SETUP
#define CAPS_WORD_IDLE_TIMEOUT 5000
