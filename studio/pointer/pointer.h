#pragma once
#include <stdbool.h>
#include <stdint.h>
struct ksp_report { uint8_t buttons; int8_t x,y,h,v; };
typedef void (*ksp_send)(void *,struct ksp_report);
struct ksp_state { uint8_t held[19]; uint32_t due, started, wheel_due; };
bool ksp_key(struct ksp_state *,uint16_t,bool,uint32_t,ksp_send,void *);
void ksp_tick(struct ksp_state *,uint32_t,ksp_send,void *);
void ksp_clear(struct ksp_state *,ksp_send,void *);
