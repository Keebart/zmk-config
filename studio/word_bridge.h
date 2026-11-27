#pragma once
#include <stdbool.h>
#include <stdint.h>
const uint8_t *ks_typing_config(void);
bool ks_word_process(uint16_t code, bool down);
bool ks_auto_shift_process(uint16_t code, bool down);
void ks_word_tick(void);
int64_t ks_word_deadline(void);
void ks_word_cancel(void);
bool ks_typing_character(uint16_t code, bool down, bool resolved);
bool ks_word_active(void);
