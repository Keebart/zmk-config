#pragma once
#include <stdbool.h>
#include <stdint.h>
#define KSH_BYTES 16
#define KSH_COMPACT 1u
#define KSH_LEFT_DISPLAY 2u
#define KSH_RIGHT_DISPLAY 4u
bool ksh_valid(uint16_t model, uint8_t flags);
uint8_t ksh_decode(const uint8_t record[KSH_BYTES], uint16_t model);
uint32_t ksh_crc(const uint8_t *bytes, unsigned length);
/* Canonical LED index for a physical chain position; 255 means no LED. */
uint8_t ksh_led(uint16_t model, uint8_t flags, uint8_t index);
/* Implemented by the hardware port, backed by an immutable application record. */
uint8_t ksh_flags(void);
uint16_t ksh_compact_key(uint16_t model,uint8_t flags,unsigned layer,unsigned cell,uint16_t fallback);
uint16_t ksh_compact_position(uint16_t model,uint8_t flags,unsigned index,uint16_t fallback);
bool ksh_compact_locale(uint16_t model,uint8_t flags,uint8_t *record);
