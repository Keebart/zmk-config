#include "pointer.h"
#include <string.h>
static void accelerated_key(struct ksp_state *,unsigned,bool,uint32_t,ksp_send,void *);
static void accelerated_tick(struct ksp_state *,uint32_t,ksp_send,void *);
void ksp_configure(struct ksp_state *s,const uint8_t *config){
 s->accelerated=config!=NULL;if(config)memcpy(s->config,config,10);
}
static bool moving(const struct ksp_state *s){return s->held[0]||s->held[1]||s->held[2]||s->held[3]||s->held[12]||s->held[13]||s->held[14]||s->held[15];}
static struct ksp_report report(struct ksp_state *s,uint32_t now,bool move){
    struct ksp_report r={0};for(unsigned i=0;i<8;i++)if(s->held[i+4])r.buttons|=1u<<i;
    if(!move)return r;
    int step=s->held[16]?1:s->held[17]?4:s->held[18]?12:1+(now-s->started)/160;
    if(step>12)step=12;
    r.x=((s->held[3]!=0)-(s->held[2]!=0))*step;
    r.y=((s->held[1]!=0)-(s->held[0]!=0))*step;
    if(r.x&&r.y){r.x=r.x*181/256;r.y=r.y*181/256;if(!r.x)r.x=s->held[3]?1:-1;if(!r.y)r.y=s->held[1]?1:-1;}
    // Wheel cadence is independent of pointer acceleration.
    if((int32_t)(now-s->wheel_due)>=0){s->wheel_due=now+100;r.v=(s->held[12]!=0)-(s->held[13]!=0);r.h=(s->held[15]!=0)-(s->held[14]!=0);}
    return r;
}
bool ksp_key(struct ksp_state *s,uint16_t code,bool down,uint32_t now,ksp_send send,void *ctx){
    if(code<0xcd||code>0xdf)return false;
    if(s->accelerated){accelerated_key(s,code-0xcd,down,now,send,ctx);return true;}
    unsigned i=code-0xcd;bool was=moving(s);
    if(down){if(s->held[i]<255)s->held[i]++;}else if(s->held[i])s->held[i]--;
    bool active=moving(s);if(!was&&active){s->started=now;s->due=now+10;s->wheel_due=now;}
    send(ctx,report(s,now,!was&&active));return true;
}
void ksp_jiggle(struct ksp_state *s,bool down,uint32_t now){
 if(!down)return;
 s->jiggle_enabled=!s->jiggle_enabled;s->jiggle_negative=false;s->jiggle_due=now+1000;
}
uint32_t ksp_jiggle_delay(const struct ksp_state *s,uint32_t now){
 if(!s->jiggle_enabled)return 0;
 int32_t remaining=(int32_t)(s->jiggle_due-now);return remaining>0?(uint32_t)remaining:1;
}
static void jiggle_tick(struct ksp_state *s,uint32_t now,ksp_send send,void *ctx){
 if(!s->jiggle_enabled||(int32_t)(now-s->jiggle_due)<0)return;
 s->jiggle_due=now+KSP_JIGGLE_INTERVAL;
 // Never add movement to a held mouse button, scroll or navigation action.
 for(unsigned i=0;i<19;i++)if(s->held[i])return;
 send(ctx,(struct ksp_report){.x=s->jiggle_negative?-1:1});
 send(ctx,(struct ksp_report){0});
 s->jiggle_negative=!s->jiggle_negative;
}
void ksp_tick(struct ksp_state *s,uint32_t now,ksp_send send,void *ctx){
    jiggle_tick(s,now,send,ctx);
    if(s->accelerated){accelerated_tick(s,now,send,ctx);return;}
    if(!moving(s)||(int32_t)(now-s->due)<0)return;
    s->due=now+10;send(ctx,report(s,now,true));
}
void ksp_clear(struct ksp_state *s,ksp_send send,void *ctx){memset(s,0,sizeof(*s));send(ctx,(struct ksp_report){0});}

/* Accelerated algorithm adapted from QMK quantum/mousekey.c.
 * Copyright 2011 Jun Wako <wakojun@gmail.com>, GPL-2.0-or-later.
 * Keep first-motion, independent wheel timing, rounding and last-direction
 * semantics identical to QMK. Tests compare reports with its native engine. */
static int8_t diagonal(int value){int n=value*181;return (n+(n<0?-128:128))/256;}
static uint8_t unit(const struct ksp_state *s,bool wheel){
 const uint8_t *c=s->config+(wheel?5:0);unsigned repeat=wheel?s->wheel_repeat:s->repeat;
 unsigned value;
 if(s->held[16])value=c[2]*c[3]/4;
 else if(s->held[17])value=c[2]*c[3]/2;
 else if(s->held[18])value=c[2]*c[3];
 else if(!repeat)value=c[2];
 else if(repeat>=c[4])value=c[2]*c[3];
 else value=c[2]*c[3]*repeat/c[4];
 return value>127?127:value?value:1;
}
static void accelerated_send(struct ksp_state *s,struct ksp_report r,uint32_t now,ksp_send send,void *ctx){
 if(r.x||r.y)s->due=now;
 if(r.v||r.h)s->wheel_due=now;
 send(ctx,r);
}
static void accelerated_key(struct ksp_state *s,unsigned i,bool down,uint32_t now,ksp_send send,void *ctx){
 if(down){if(s->held[i]<255)s->held[i]++;}else if(s->held[i])s->held[i]--;
 if(!down&&s->held[i])return;
 struct ksp_report *r=&s->motion;
 int8_t *axis=i<2?&r->y:i<4?&r->x:i<14?&r->v:&r->h;
 int sign=(i==0||i==2||i==13||i==14)?-1:1;
 if(i<4||(i>=12&&i<16)){
  if(down)*axis=sign*unit(s,i>=12);
  else if((*axis>0&&sign>0)||(*axis<0&&sign<0))*axis=0;
 }else if(i<12){if(down)r->buttons|=1u<<(i-4);else r->buttons&=~(1u<<(i-4));}
 if(!r->x&&!r->y)s->repeat=0;
 if(!r->h&&!r->v)s->wheel_repeat=0;
 accelerated_send(s,*r,now,send,ctx);
}
static void accelerated_tick(struct ksp_state *s,uint32_t now,ksp_send send,void *ctx){
 struct ksp_report r={.buttons=s->motion.buttons};
 const struct ksp_report *held=&s->motion;
 if((held->x||held->y)&&(uint16_t)(now-s->due)>(s->repeat?s->config[1]:s->config[0]*10)){
  if(s->repeat<255)s->repeat++;
  int step=unit(s,false);
  r.x=held->x?step*(held->x>0?1:-1):0;r.y=held->y?step*(held->y>0?1:-1):0;
  if(r.x&&r.y){r.x=diagonal(r.x);r.y=diagonal(r.y);}
 }
 if((held->v||held->h)&&(uint16_t)(now-s->wheel_due)>(s->wheel_repeat?s->config[6]:s->config[5]*10)){
  if(s->wheel_repeat<255)s->wheel_repeat++;
  int step=unit(s,true);
  r.h=held->h?step*(held->h>0?1:-1):0;r.v=held->v?step*(held->v>0?1:-1):0;
  if(r.h&&r.v){r.h=diagonal(r.h);r.v=diagonal(r.v);}
 }
 if(r.x||r.y||r.h||r.v)accelerated_send(s,r,now,send,ctx);
}
