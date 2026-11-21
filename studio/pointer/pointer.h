#pragma once
#include <stdbool.h>
#include <stdint.h>
struct ksp_report { uint8_t buttons; int8_t x,y,h,v; };
typedef void (*ksp_send)(void *,struct ksp_report);
struct ksp_state {
 bool jiggle_enabled,jiggle_negative; uint32_t jiggle_due;
 uint8_t held[19]; uint32_t due, started, wheel_due;
 uint8_t config[10],repeat,wheel_repeat; bool accelerated;
 struct ksp_report motion;
};
/* Optional QMK accelerated mode. NULL retains Studio's original pointer mode. */
void ksp_configure(struct ksp_state *,const uint8_t *);
bool ksp_key(struct ksp_state *,uint16_t,bool,uint32_t,ksp_send,void *);
void ksp_tick(struct ksp_state *,uint32_t,ksp_send,void *);
void ksp_clear(struct ksp_state *,ksp_send,void *);

#define KSP_JIGGLE_INTERVAL 30000u
void ksp_jiggle(struct ksp_state *,bool,uint32_t);
uint32_t ksp_jiggle_delay(const struct ksp_state *,uint32_t);
