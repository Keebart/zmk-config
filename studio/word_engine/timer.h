#pragma once
#include <stdbool.h>
#include <stdint.h>
uint32_t timer_read32(void);
static inline uint16_t timer_read(void) { return (uint16_t)timer_read32(); }
static inline uint32_t timer_elapsed32(uint32_t last) { return timer_read32() - last; }
static inline uint16_t timer_elapsed(uint16_t last) { return (uint16_t)(timer_read() - last); }
static inline bool timer_expired(uint16_t current, uint16_t future) { return (uint16_t)(current - future) < 0x8000u; }
static inline bool timer_expired32(uint32_t current, uint32_t future) { return (uint32_t)(current - future) < 0x80000000u; }
#define TIMER_DIFF_16(a, b) ((uint16_t)((a) - (b)))
